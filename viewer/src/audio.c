#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "net.h"
#include "protocol.h"

/* Keep at most this much audio queued; older samples are dropped to bound latency. */
#define MAX_QUEUED_MS 150

struct sdv_audio {
    char host[128], device_id[64], name[128];
    int port;
    struct sdv_conn conn;
    int conn_open;
    SDL_Thread *thread;
    SDL_atomic_t running;
    SDL_atomic_t status;
    SDL_mutex *lock;
    char error[128];
    SDL_AudioDeviceID dev;
    struct sd_audio_format fmt;
};

static void fail(struct sdv_audio *a, const char *msg)
{
    fprintf(stderr, "audio: %s (%s)\n", msg, SDL_GetError());
    SDL_LockMutex(a->lock);
    snprintf(a->error, sizeof(a->error), "%s", msg);
    SDL_UnlockMutex(a->lock);
    SDL_AtomicSet(&a->status, -1);
}

static int open_device(struct sdv_audio *a, const struct sd_audio_format *f)
{
    SDL_AudioSpec want = { 0 }, have;

    if (a->dev)
        SDL_CloseAudioDevice(a->dev);
    want.freq = (int)f->rate;
    want.format = f->bits == 32 ? AUDIO_S32LSB : AUDIO_S16LSB;
    want.channels = (Uint8)f->channels;
    want.samples = 512;
    a->dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!a->dev)
        return -1;
    a->fmt = *f;
    SDL_PauseAudioDevice(a->dev, 0);
    fprintf(stderr, "audio: playing %u Hz, %u ch, %u bit via %s\n", f->rate, f->channels, f->bits,
            SDL_GetCurrentAudioDriver());
    return 0;
}

static int audio_thread(void *arg)
{
    struct sdv_audio *a = arg;
    uint8_t ident[SD_HEADER_LEN + SD_IDENT_PAYLOAD_LEN], hdr[SD_HEADER_LEN];
    uint8_t *buf = NULL;
    size_t cap = 0;

    if (sdv_conn_open(&a->conn, a->host, a->port) < 0) {
        fail(a, "Audio: no se pudo conectar");
        return 0;
    }
    a->conn_open = 1;
    sd_build_speaker_identification(ident, a->device_id, a->name);
    if (sdv_conn_send(&a->conn, ident, sizeof(ident)) < 0) {
        fail(a, "Audio: error de identificacion");
        return 0;
    }

    while (SDL_AtomicGet(&a->running)) {
        uint32_t type, len;
        struct sd_audio_format f;

        if (sdv_conn_recv(&a->conn, hdr, sizeof(hdr)) < 0)
            break;
        type = sd_get_u32(hdr);
        len = sd_get_u32(hdr + 4);
        if (len > 4u * 1024 * 1024)
            break;
        if (len > cap) {
            uint8_t *nb = realloc(buf, len);
            if (!nb)
                break;
            buf = nb;
            cap = len;
        }
        if (len && sdv_conn_recv(&a->conn, buf, len) < 0)
            break;
        if (type == SD_T_ERROR)
            break;
        if (type == SD_T_PING && sd_get_u32(hdr + 12) == SD_PING_REQUEST) {
            uint8_t pong[SD_HEADER_LEN];
            sd_build_ping_reply(pong);
            sdv_conn_send(&a->conn, pong, sizeof(pong));
            continue;
        }
        if (type != SD_T_AUDIO_SPEAKER || !len)
            continue;

        sd_parse_audio(hdr, &f);
        if (!a->dev || f.rate != a->fmt.rate || f.channels != a->fmt.channels || f.bits != a->fmt.bits) {
            if (open_device(a, &f) < 0) {
                fail(a, "Audio: no hay dispositivo de salida");
                break;
            }
            SDL_AtomicSet(&a->status, 1);
        }
        {
            Uint32 bytes_per_ms = f.rate * f.channels * (f.bits / 8) / 1000;
            if (SDL_GetQueuedAudioSize(a->dev) > bytes_per_ms * MAX_QUEUED_MS)
                SDL_ClearQueuedAudio(a->dev);
            SDL_QueueAudio(a->dev, buf, len);
        }
    }
    free(buf);
    if (SDL_AtomicGet(&a->running) && SDL_AtomicGet(&a->status) != -1)
        fail(a, "Audio desconectado");
    return 0;
}

struct sdv_audio *sdv_audio_start(const char *host, int port, const char *device_id, const char *name)
{
    struct sdv_audio *a = calloc(1, sizeof(*a));

    if (!a)
        return NULL;
    snprintf(a->host, sizeof(a->host), "%s", host);
    snprintf(a->device_id, sizeof(a->device_id), "%s", device_id);
    snprintf(a->name, sizeof(a->name), "%s", name);
    a->port = port;
    a->conn.fd = -1;
    a->lock = SDL_CreateMutex();
    if (!a->lock || (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)) {
        if (a->lock)
            SDL_DestroyMutex(a->lock);
        free(a);
        return NULL;
    }
    SDL_AtomicSet(&a->running, 1);
    a->thread = SDL_CreateThread(audio_thread, "sdv-audio", a);
    if (!a->thread) {
        SDL_DestroyMutex(a->lock);
        free(a);
        return NULL;
    }
    return a;
}

void sdv_audio_stop(struct sdv_audio *a)
{
    uint8_t bye[SD_DISCONNECT_LEN];

    if (!a)
        return;
    SDL_AtomicSet(&a->running, 0);
    if (a->conn_open) {
        sd_build_disconnect(bye);
        sdv_conn_send(&a->conn, bye, sizeof(bye));
        sdv_conn_shutdown(&a->conn);
    }
    SDL_WaitThread(a->thread, NULL);
    sdv_conn_close(&a->conn);
    if (a->dev)
        SDL_CloseAudioDevice(a->dev);
    SDL_DestroyMutex(a->lock);
    free(a);
}

int sdv_audio_status(struct sdv_audio *a, char *err, int errlen)
{
    int st = SDL_AtomicGet(&a->status);
    if (err && errlen > 0) {
        SDL_LockMutex(a->lock);
        snprintf(err, (size_t)errlen, "%s", a->error);
        SDL_UnlockMutex(a->lock);
    }
    return st;
}
