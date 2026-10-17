#include "session.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "net.h"

struct sdv_session {
    struct sdv_session_config cfg;
    Uint32 frame_event, state_event;
    struct sdv_conn conn;
    int conn_open;
    SDL_Thread *thread;
    SDL_atomic_t running;
    SDL_atomic_t target_w, target_h;

    SDL_mutex *lock; /* guards everything below */
    enum sdv_session_state state;
    char error[160];
    struct sdv_image images[2];
    int front;
    int fresh;
    struct sdv_session_stats stats;
};

static void push_event(Uint32 type)
{
    SDL_Event ev = { 0 };
    ev.type = type;
    SDL_PushEvent(&ev);
}

static void set_state(struct sdv_session *s, enum sdv_session_state st, const char *err)
{
    SDL_LockMutex(s->lock);
    s->state = st;
    if (err)
        snprintf(s->error, sizeof(s->error), "%s", err);
    SDL_UnlockMutex(s->lock);
    push_event(s->state_event);
}

int sdv_session_send(struct sdv_session *s, const void *buf, size_t len)
{
    int r;

    if (!s || !s->conn_open)
        return -1;
    r = sdv_conn_send(&s->conn, buf, len);
    if (r == 0) {
        SDL_LockMutex(s->lock);
        s->stats.bytes_tx += len;
        SDL_UnlockMutex(s->lock);
    }
    return r;
}

static void fill_addresses(struct sdv_session *s)
{
    struct sockaddr_storage ss;
    socklen_t sl = sizeof(ss);
    char ip[INET6_ADDRSTRLEN] = "?";

    if (getsockname(s->conn.fd, (struct sockaddr *)&ss, &sl) == 0) {
        if (ss.ss_family == AF_INET)
            inet_ntop(AF_INET, &((struct sockaddr_in *)&ss)->sin_addr, ip, sizeof(ip));
        else if (ss.ss_family == AF_INET6)
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&ss)->sin6_addr, ip, sizeof(ip));
    }
    snprintf(s->stats.local_addr, sizeof(s->stats.local_addr), "%s", ip);
    snprintf(s->stats.remote_addr, sizeof(s->stats.remote_addr), "%s:%d", s->cfg.host, s->cfg.port);
}

static int session_thread(void *arg)
{
    struct sdv_session *s = arg;
    struct sdv_decoder *dec = sdv_decoder_new(s->cfg.allow_hw);
    uint8_t ident[SD_HEADER_LEN + SD_IDENT_PAYLOAD_LEN], hdr[SD_HEADER_LEN], ack[SD_HEADER_LEN];
    uint8_t *payload = NULL;
    size_t cap = 0;
    char err[160] = "";

    if (!dec) {
        set_state(s, SDV_STATE_FAILED, "out of memory");
        return 0;
    }
    if (sdv_conn_open(&s->conn, s->cfg.host, s->cfg.port) < 0) {
        snprintf(err, sizeof(err), "No se pudo conectar a %s:%d", s->cfg.host, s->cfg.port);
        sdv_decoder_free(dec);
        set_state(s, SDV_STATE_FAILED, err);
        return 0;
    }
    s->conn_open = 1;
    fill_addresses(s);

    s->cfg.ident.device_id = s->cfg.device_id;
    s->cfg.ident.name = s->cfg.name;
    sd_build_identification(ident, &s->cfg.ident);
    if (sdv_session_send(s, ident, sizeof(ident)) < 0) {
        sdv_decoder_free(dec);
        set_state(s, SDV_STATE_FAILED, "Error enviando identificacion");
        return 0;
    }
    set_state(s, SDV_STATE_STREAMING, NULL);

    sd_build_flow_ack(ack);
    while (SDL_AtomicGet(&s->running)) {
        uint32_t type, len;
        Uint64 tw0 = SDL_GetPerformanceCounter();

        if (sdv_conn_recv(&s->conn, hdr, sizeof(hdr)) < 0) {
            snprintf(err, sizeof(err), "Conexion cerrada por el servidor");
            break;
        }
        type = sd_get_u32(hdr);
        len = sd_get_u32(hdr + 4);
        if (len > 64u * 1024 * 1024) {
            snprintf(err, sizeof(err), "Paquete invalido (%u bytes)", len);
            break;
        }
        if (len > cap) {
            uint8_t *nb = realloc(payload, len);
            if (!nb)
                break;
            payload = nb;
            cap = len;
        }
        if (len && sdv_conn_recv(&s->conn, payload, len) < 0) {
            snprintf(err, sizeof(err), "Conexion cerrada por el servidor");
            break;
        }
        SDL_LockMutex(s->lock);
        s->stats.bytes_rx += len + SD_HEADER_LEN;
        s->stats.t_wait_us += (SDL_GetPerformanceCounter() - tw0) * 1000000 / SDL_GetPerformanceFrequency();
        if (type == SD_T_FRAMEBUFFER && len > s->stats.max_frame_bytes)
            s->stats.max_frame_bytes = len;
        SDL_UnlockMutex(s->lock);

        if (type == SD_T_FRAMEBUFFER) {
            struct sd_framebuffer fb;
            struct sdv_image *back;
            int r;

            sd_parse_framebuffer(hdr, &fb);
            /* Flow control: the server sends the next frame only after an ack. Ack an H.264
             * access unit as soon as it is received so the server encodes the next frame
             * while this one decodes. */
            if (fb.encoding == SD_ENC_H264)
                sdv_session_send(s, ack, sizeof(ack));
            /* Only this thread writes the back image; the render loop reads the front one. */
            SDL_LockMutex(s->lock);
            back = &s->images[s->front ^ 1];
            SDL_UnlockMutex(s->lock);
            sdv_decoder_set_target(dec, SDL_AtomicGet(&s->target_w), SDL_AtomicGet(&s->target_h));
            r = sdv_decoder_feed(dec, &fb, payload, len, back);
            if (r > 0) {
                SDL_LockMutex(s->lock);
                s->front ^= 1;
                s->fresh = 1;
                s->stats.frames++;
                s->stats.encoding = fb.encoding;
                s->stats.remote_w = back->src_w;
                s->stats.remote_h = back->src_h;
                s->stats.h264_backend = sdv_decoder_h264_backend(dec);
                sdv_decoder_timing(dec, &s->stats.t_decode_us, &s->stats.t_xfer_us);
                SDL_UnlockMutex(s->lock);
                push_event(s->frame_event);
            }
            /* Fragmented formats: one ack per complete frame (on the last fragment). */
            if (r > 0 && fb.encoding != SD_ENC_H264)
                sdv_session_send(s, ack, sizeof(ack));
        } else if (type == SD_T_PING && sd_get_u32(hdr + 12) == SD_PING_REQUEST) {
            /* a still desktop sends no frames, so no acks either: answer to stay connected */
            uint8_t pong[SD_HEADER_LEN];
            sd_build_ping_reply(pong);
            sdv_session_send(s, pong, sizeof(pong));
        } else if (type == SD_T_ERROR) {
            snprintf(err, sizeof(err), "El servidor cerro la sesion (motivo %u)", sd_get_u32(hdr + 8));
            break;
        }
    }

    free(payload);
    sdv_decoder_free(dec);
    if (SDL_AtomicGet(&s->running))
        set_state(s, SDV_STATE_CLOSED, err[0] ? err : "Desconectado");
    return 0;
}

struct sdv_session *sdv_session_start(const struct sdv_session_config *cfg,
                                      Uint32 frame_event, Uint32 state_event)
{
    struct sdv_session *s = calloc(1, sizeof(*s));

    if (!s)
        return NULL;
    s->cfg = *cfg;
    s->frame_event = frame_event;
    s->state_event = state_event;
    s->conn.fd = -1;
    s->lock = SDL_CreateMutex();
    if (!s->lock) {
        free(s);
        return NULL;
    }
    s->state = SDV_STATE_CONNECTING;
    SDL_AtomicSet(&s->running, 1);
    s->thread = SDL_CreateThread(session_thread, "sdv-session", s);
    if (!s->thread) {
        SDL_DestroyMutex(s->lock);
        free(s);
        return NULL;
    }
    return s;
}

void sdv_session_stop(struct sdv_session *s)
{
    uint8_t bye[SD_DISCONNECT_LEN];

    if (!s)
        return;
    SDL_AtomicSet(&s->running, 0);
    if (s->conn_open) {
        sd_build_disconnect(bye);
        sdv_conn_send(&s->conn, bye, sizeof(bye));
        sdv_conn_shutdown(&s->conn);
    }
    /* A thread still blocked in connect() finishes on its own timeout. */
    SDL_WaitThread(s->thread, NULL);
    sdv_conn_close(&s->conn);
    sdv_image_free(&s->images[0]);
    sdv_image_free(&s->images[1]);
    SDL_DestroyMutex(s->lock);
    free(s);
}

enum sdv_session_state sdv_session_state(struct sdv_session *s, char *err, size_t errlen)
{
    enum sdv_session_state st;

    SDL_LockMutex(s->lock);
    st = s->state;
    if (err && errlen)
        snprintf(err, errlen, "%s", s->error);
    SDL_UnlockMutex(s->lock);
    return st;
}

int sdv_session_take_frame(struct sdv_session *s,
                           void (*fn)(void *user, const struct sdv_image *img), void *user)
{
    int called = 0;

    SDL_LockMutex(s->lock);
    if (s->fresh) {
        fn(user, &s->images[s->front]);
        s->fresh = 0;
        called = 1;
    }
    SDL_UnlockMutex(s->lock);
    return called;
}

void sdv_session_get_stats(struct sdv_session *s, struct sdv_session_stats *out)
{
    SDL_LockMutex(s->lock);
    *out = s->stats;
    SDL_UnlockMutex(s->lock);
}

void sdv_session_set_target(struct sdv_session *s, int w, int h)
{
    SDL_AtomicSet(&s->target_w, w);
    SDL_AtomicSet(&s->target_h, h);
}
