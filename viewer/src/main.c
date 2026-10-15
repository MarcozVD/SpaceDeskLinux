/* sdviewer: native Linux viewer for a spacedesk server. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL.h>

#include "decoder.h"
#include "keymap.h"
#include "net.h"
#include "protocol.h"

#define SDV_VERSION "0.1.0"
#define OS_TYPE_UNKNOWN 0

struct options {
    const char *host;
    int port;
    int width, height; /* 0 = size of the target display */
    int fps;
    int quality;
    int encoding;
    int allow_hw;
    int windowed;
    int display;
    int verbose;
};

/* Latest decoded frame, handed from the network thread to the render loop. */
struct frame_slot {
    SDL_mutex *lock;
    struct sdv_image images[2];
    int front;
    int fresh;
};

struct app {
    struct options opt;
    struct sdv_conn conn;
    struct sdv_decoder *dec;
    struct frame_slot slot;
    Uint32 frame_event;
    SDL_atomic_t running;
    SDL_atomic_t frames_rx;
    SDL_atomic_t bytes_rx;
    int remote_w, remote_h; /* render thread only */
};

/* ---------- device identity ---------- */

static void config_path(char *out, size_t n, const char *leaf)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(out, n, "%s/sdviewer%s%s", xdg, leaf ? "/" : "", leaf ? leaf : "");
    else
        snprintf(out, n, "%s/.config/sdviewer%s%s", home ? home : ".", leaf ? "/" : "", leaf ? leaf : "");
}

/* One stable GUID per installation: the server keys its per-device settings on it. */
static void load_device_id(char *out, size_t n)
{
    char dir[512], path[600];
    unsigned char r[16];
    FILE *f;
    int fd;

    config_path(path, sizeof(path), "device-id");
    f = fopen(path, "r");
    if (f) {
        if (fgets(out, (int)n, f)) {
            out[strcspn(out, "\r\n")] = 0;
            fclose(f);
            if (out[0] == '{')
                return;
        } else {
            fclose(f);
        }
    }
    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0 || read(fd, r, sizeof(r)) != (ssize_t)sizeof(r)) {
        for (size_t i = 0; i < sizeof(r); i++)
            r[i] = (unsigned char)(rand() ^ (int)SDL_GetPerformanceCounter());
    }
    if (fd >= 0)
        close(fd);
    r[6] = (r[6] & 0x0F) | 0x40;
    r[8] = (r[8] & 0x3F) | 0x80;
    snprintf(out, n,
             "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
             r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);

    config_path(dir, sizeof(dir), NULL);
    mkdir(dir, 0755);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", out);
        fclose(f);
    }
}

/* ---------- network thread ---------- */

static void send_packet(struct app *a, const void *buf, size_t len)
{
    if (sdv_conn_send(&a->conn, buf, len) < 0 && SDL_AtomicGet(&a->running))
        fprintf(stderr, "net: send failed\n");
}

static void publish_frame(struct app *a)
{
    SDL_Event ev = { 0 };

    SDL_LockMutex(a->slot.lock);
    a->slot.front ^= 1;
    a->slot.fresh = 1;
    SDL_UnlockMutex(a->slot.lock);

    ev.type = a->frame_event;
    SDL_PushEvent(&ev);
}

static int net_thread(void *arg)
{
    struct app *a = arg;
    uint8_t hdr[SD_HEADER_LEN], ack[SD_HEADER_LEN];
    uint8_t *payload = NULL;
    size_t cap = 0;

    sd_build_flow_ack(ack);
    while (SDL_AtomicGet(&a->running)) {
        uint32_t type, len;

        if (sdv_conn_recv(&a->conn, hdr, sizeof(hdr)) < 0)
            break;
        type = sd_get_u32(hdr);
        len = sd_get_u32(hdr + 4);
        if (len > 64u * 1024 * 1024) {
            fprintf(stderr, "net: implausible payload length %u, closing\n", len);
            break;
        }
        if (len > cap) {
            uint8_t *nb = realloc(payload, len);
            if (!nb)
                break;
            payload = nb;
            cap = len;
        }
        if (len && sdv_conn_recv(&a->conn, payload, len) < 0)
            break;
        SDL_AtomicAdd(&a->bytes_rx, (int)(len + SD_HEADER_LEN));

        switch (type) {
        case SD_T_FRAMEBUFFER: {
            struct sd_framebuffer fb;
            struct sdv_image *back;
            int r;

            sd_parse_framebuffer(hdr, &fb);
            /* Only this thread writes the back image; the render loop reads the front one. */
            SDL_LockMutex(a->slot.lock);
            back = &a->slot.images[a->slot.front ^ 1];
            SDL_UnlockMutex(a->slot.lock);
            r = sdv_decoder_feed(a->dec, &fb, payload, len, back);
            if (r > 0) {
                publish_frame(a);
                SDL_AtomicAdd(&a->frames_rx, 1);
            }
            /* Flow control: one ack per complete frame (fragments only ack on the last one). */
            if (r > 0 || fb.encoding == SD_ENC_H264)
                send_packet(a, ack, sizeof(ack));
            break;
        }
        case SD_T_VISIBILITY:
            if (a->opt.verbose)
                fprintf(stderr, "server: visibility %u\n", sd_get_u32(hdr + 8));
            break;
        case SD_T_ERROR:
            fprintf(stderr, "server: closed session (reason %u)\n", sd_get_u32(hdr + 8));
            goto out;
        default:
            if (a->opt.verbose)
                fprintf(stderr, "server: %s (%u bytes)\n", sd_type_name(type), len);
            break;
        }
    }
out:
    if (SDL_AtomicGet(&a->running)) {
        SDL_Event ev = { .type = SDL_QUIT };
        fprintf(stderr, "net: connection lost\n");
        SDL_PushEvent(&ev);
    }
    free(payload);
    return 0;
}

/* ---------- input ---------- */

static Uint32 now_ms(void)
{
    return SDL_GetTicks() % 1000;
}

/* With SDL_RenderSetLogicalSize active, SDL already rewrites mouse coordinates to
 * logical (= remote desktop) pixels and finger coordinates to 0..1 of the logical area. */
static int to_remote(const struct app *a, float lx, float ly, int *rx, int *ry)
{
    if (a->remote_w <= 0 || a->remote_h <= 0)
        return -1;
    if (lx < 0 || ly < 0 || lx >= a->remote_w || ly >= a->remote_h)
        return -1;
    *rx = (int)lx;
    *ry = (int)ly;
    return 0;
}

/* Absolute pointer position: the server ignores MOUSEEVENTF_ABSOLUTE and applies
 * pointer acceleration to relative moves, but a hovering touch contact is mapped
 * absolutely onto the virtual monitor (see docs/protocol.md). */
static void send_pointer(struct app *a, int x, int y)
{
    uint8_t pkt[SD_HEADER_LEN];
    sd_build_touch(pkt, x, y, (uint32_t)a->remote_w, (uint32_t)a->remote_h, SD_TOUCH_ABSOLUTE, 0, now_ms());
    send_packet(a, pkt, sizeof(pkt));
}

static void send_buttons(struct app *a, uint32_t flags, int32_t data)
{
    uint8_t pkt[SD_HEADER_LEN];
    sd_build_mouse(pkt, 0, 0, data, flags);
    send_packet(a, pkt, sizeof(pkt));
}

static uint32_t button_flags(Uint8 button, int down)
{
    switch (button) {
    case SDL_BUTTON_LEFT: return down ? SD_MOUSE_LEFTDOWN : SD_MOUSE_LEFTUP;
    case SDL_BUTTON_RIGHT: return down ? SD_MOUSE_RIGHTDOWN : SD_MOUSE_RIGHTUP;
    case SDL_BUTTON_MIDDLE: return down ? SD_MOUSE_MIDDLEDOWN : SD_MOUSE_MIDDLEUP;
    default: return 0;
    }
}

static void handle_key(struct app *a, const SDL_KeyboardEvent *k)
{
    struct sdv_key key;
    uint8_t pkt[SD_HEADER_LEN];
    uint32_t flags = SD_KEY_SCANCODE;

    if (sdv_keymap_lookup(k->keysym.scancode, &key) < 0) {
        if (a->opt.verbose)
            fprintf(stderr, "input: unmapped key %s\n", SDL_GetScancodeName(k->keysym.scancode));
        return;
    }
    if (key.extended)
        flags |= SD_KEY_EXTENDED;
    if (k->type == SDL_KEYUP)
        flags |= SD_KEY_UP;
    sd_build_keyboard(pkt, key.vk, key.scan, flags);
    send_packet(a, pkt, sizeof(pkt));
}

/* Local hotkeys (not forwarded): Ctrl+Alt+Q quit, Ctrl+Alt+F toggle fullscreen. */
static int handle_hotkey(SDL_Window *win, const SDL_KeyboardEvent *k)
{
    SDL_Keymod mod = SDL_GetModState();

    if (k->type != SDL_KEYDOWN || !(mod & KMOD_CTRL) || !(mod & KMOD_ALT))
        return 0;
    if (k->keysym.scancode == SDL_SCANCODE_Q) {
        SDL_Event ev = { .type = SDL_QUIT };
        SDL_PushEvent(&ev);
        return 1;
    }
    if (k->keysym.scancode == SDL_SCANCODE_F) {
        Uint32 fs = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_SetWindowFullscreen(win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        return 1;
    }
    return 0;
}

/* ---------- rendering ---------- */

static SDL_Texture *upload(struct app *a, SDL_Renderer *ren, SDL_Texture *tex, const struct sdv_image *img)
{
    static const Uint32 sdl_fmt[] = {
        [SDV_FMT_BGRX] = SDL_PIXELFORMAT_RGB888,
        [SDV_FMT_NV12] = SDL_PIXELFORMAT_NV12,
        [SDV_FMT_I420] = SDL_PIXELFORMAT_IYUV,
    };
    Uint32 fmt;
    int tw = 0, th = 0;

    if (img->fmt == SDV_FMT_NONE)
        return tex;
    if (tex)
        SDL_QueryTexture(tex, &fmt, NULL, &tw, &th);
    if (!tex || fmt != sdl_fmt[img->fmt] || tw != img->width || th != img->height) {
        if (tex)
            SDL_DestroyTexture(tex);
        tex = SDL_CreateTexture(ren, sdl_fmt[img->fmt], SDL_TEXTUREACCESS_STREAMING,
                                img->width, img->height);
        if (!tex) {
            fprintf(stderr, "render: SDL_CreateTexture: %s\n", SDL_GetError());
            return NULL;
        }
        SDL_RenderSetLogicalSize(ren, img->width, img->height);
        a->remote_w = img->width;
        a->remote_h = img->height;
        fprintf(stderr, "render: remote desktop %dx%d\n", img->width, img->height);
    }
    switch (img->fmt) {
    case SDV_FMT_BGRX:
        SDL_UpdateTexture(tex, NULL, img->plane[0], img->pitch[0]);
        break;
    case SDV_FMT_NV12:
        SDL_UpdateNVTexture(tex, NULL, img->plane[0], img->pitch[0], img->plane[1], img->pitch[1]);
        break;
    case SDV_FMT_I420:
        SDL_UpdateYUVTexture(tex, NULL, img->plane[0], img->pitch[0], img->plane[1], img->pitch[1],
                             img->plane[2], img->pitch[2]);
        break;
    default:
        break;
    }
    return tex;
}

/* ---------- main ---------- */

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [options] <server-host>\n"
            "  -p, --port N         server port (default %d)\n"
            "  -s, --size WxH       requested desktop size (default: target display size)\n"
            "  -f, --fps N          frames per second (default 60)\n"
            "  -q, --quality N      encoder quality 1-100 (default 70)\n"
            "  -j, --jpeg           use JPEG instead of H.264\n"
            "      --no-hw          disable VA-API decoding\n"
            "  -w, --windowed       start in a window instead of fullscreen\n"
            "  -d, --display N      SDL display index for fullscreen (default 0)\n"
            "  -v, --verbose        log protocol events and statistics\n"
            "Hotkeys: Ctrl+Alt+F fullscreen, Ctrl+Alt+Q quit\n",
            argv0, SD_PORT);
}

static int parse_args(int argc, char **argv, struct options *o)
{
    static const struct option longopts[] = {
        { "port", required_argument, NULL, 'p' },
        { "size", required_argument, NULL, 's' },
        { "fps", required_argument, NULL, 'f' },
        { "quality", required_argument, NULL, 'q' },
        { "jpeg", no_argument, NULL, 'j' },
        { "no-hw", no_argument, NULL, 'H' },
        { "windowed", no_argument, NULL, 'w' },
        { "display", required_argument, NULL, 'd' },
        { "verbose", no_argument, NULL, 'v' },
        { "help", no_argument, NULL, 'h' },
        { 0 },
    };
    int c;

    *o = (struct options){ .port = SD_PORT, .fps = 60, .quality = 70,
                           .encoding = SD_ENC_H264, .allow_hw = 1 };
    while ((c = getopt_long(argc, argv, "p:s:f:q:jwd:vh", longopts, NULL)) != -1) {
        switch (c) {
        case 'p': o->port = atoi(optarg); break;
        case 's':
            if (sscanf(optarg, "%dx%d", &o->width, &o->height) != 2)
                return -1;
            break;
        case 'f': o->fps = atoi(optarg); break;
        case 'q': o->quality = atoi(optarg); break;
        case 'j': o->encoding = SD_ENC_JPEG; break;
        case 'H': o->allow_hw = 0; break;
        case 'w': o->windowed = 1; break;
        case 'd': o->display = atoi(optarg); break;
        case 'v': o->verbose = 1; break;
        default: return -1;
        }
    }
    if (optind != argc - 1)
        return -1;
    o->host = argv[optind];
    /* fps 0 and 1 are aliases for 30 and 60 on the wire */
    if (o->fps < 2)
        o->fps = 60;
    if (o->quality < 1 || o->quality > 100)
        o->quality = 70;
    return 0;
}

int main(int argc, char **argv)
{
    struct app a = { 0 };
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex = NULL;
    SDL_Thread *thr;
    SDL_DisplayMode mode;
    struct sd_ident_params ip;
    uint8_t ident[SD_HEADER_LEN + SD_IDENT_PAYLOAD_LEN], bye[SD_DISCONNECT_LEN];
    char device_id[64], name[128], host[64];
    Uint32 stats_t0;
    int done = 0;

    if (parse_args(argc, argv, &a.opt) < 0) {
        usage(argv[0]);
        return 2;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (a.opt.display >= SDL_GetNumVideoDisplays())
        a.opt.display = 0;
    if (SDL_GetDesktopDisplayMode(a.opt.display, &mode) < 0)
        mode.w = 1280, mode.h = 720;
    if (!a.opt.width || !a.opt.height) {
        a.opt.width = mode.w;
        a.opt.height = mode.h;
    }

    win = SDL_CreateWindow("sdviewer",
                           SDL_WINDOWPOS_CENTERED_DISPLAY(a.opt.display),
                           SDL_WINDOWPOS_CENTERED_DISPLAY(a.opt.display),
                           a.opt.windowed ? a.opt.width / 2 : mode.w,
                           a.opt.windowed ? a.opt.height / 2 : mode.h,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                               (a.opt.windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP));
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren)
        ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return 1;
    }
    if (a.opt.verbose) {
        SDL_RendererInfo ri;
        SDL_GetRendererInfo(ren, &ri);
        fprintf(stderr, "render: SDL video driver %s, renderer %s\n", SDL_GetCurrentVideoDriver(), ri.name);
    }
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderPresent(ren);

    a.frame_event = SDL_RegisterEvents(1);
    a.slot.lock = SDL_CreateMutex();
    a.dec = sdv_decoder_new(a.opt.allow_hw);
    if (a.frame_event == (Uint32)-1 || !a.slot.lock || !a.dec)
        return 1;

    fprintf(stderr, "net: connecting to %s:%d\n", a.opt.host, a.opt.port);
    if (sdv_conn_open(&a.conn, a.opt.host, a.opt.port) < 0)
        return 1;

    load_device_id(device_id, sizeof(device_id));
    if (gethostname(host, sizeof(host)) != 0)
        snprintf(host, sizeof(host), "linux");
    host[sizeof(host) - 1] = 0;
    snprintf(name, sizeof(name), "%s (sdviewer %s)", host, SDV_VERSION);
    ip = (struct sd_ident_params){
        .client_type = SD_CLIENT_REMOTE_MONITOR,
        .os_type = OS_TYPE_UNKNOWN,
        .encoding = (uint16_t)a.opt.encoding,
        .quality = (uint32_t)a.opt.quality,
        .fps = (uint16_t)a.opt.fps,
        .width = (uint32_t)a.opt.width,
        .height = (uint32_t)a.opt.height,
        .device_id = device_id,
        .name = name,
    };
    sd_build_identification(ident, &ip);
    if (sdv_conn_send(&a.conn, ident, sizeof(ident)) < 0) {
        fprintf(stderr, "net: identification failed\n");
        return 1;
    }
    fprintf(stderr, "net: identified as %s, %dx%d@%d, %s q%d\n", device_id, a.opt.width,
            a.opt.height, a.opt.fps, a.opt.encoding == SD_ENC_H264 ? "H.264" : "JPEG", a.opt.quality);

    SDL_AtomicSet(&a.running, 1);
    thr = SDL_CreateThread(net_thread, "sdv-net", &a);
    if (!thr)
        return 1;

    stats_t0 = SDL_GetTicks();
    while (!done) {
        SDL_Event ev;
        int redraw = 0;

        if (!SDL_WaitEventTimeout(&ev, 500))
            ev.type = SDL_FIRSTEVENT;
        do {
            int x, y;

            if (ev.type == a.frame_event) {
                redraw = 1;
                continue;
            }
            switch (ev.type) {
            case SDL_QUIT:
                done = 1;
                break;
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_EXPOSED ||
                    ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    redraw = 1;
                break;
            case SDL_MOUSEMOTION:
                if (ev.motion.which != SDL_TOUCH_MOUSEID &&
                    to_remote(&a, (float)ev.motion.x, (float)ev.motion.y, &x, &y) == 0)
                    send_pointer(&a, x, y);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                if (ev.button.which == SDL_TOUCH_MOUSEID)
                    break;
                if (to_remote(&a, (float)ev.button.x, (float)ev.button.y, &x, &y) == 0)
                    send_pointer(&a, x, y);
                if (button_flags(ev.button.button, ev.type == SDL_MOUSEBUTTONDOWN))
                    send_buttons(&a, button_flags(ev.button.button, ev.type == SDL_MOUSEBUTTONDOWN), 0);
                break;
            case SDL_MOUSEWHEEL:
                if (ev.wheel.y)
                    send_buttons(&a, SD_MOUSE_WHEEL, ev.wheel.y * SD_WHEEL_DELTA);
                if (ev.wheel.x)
                    send_buttons(&a, SD_MOUSE_HWHEEL, ev.wheel.x * SD_WHEEL_DELTA);
                break;
            case SDL_FINGERDOWN:
            case SDL_FINGERMOTION:
            case SDL_FINGERUP: {
                uint32_t flags = SD_TOUCH_ABSOLUTE;
                uint8_t pkt[SD_HEADER_LEN];

                if (to_remote(&a, ev.tfinger.x * a.remote_w, ev.tfinger.y * a.remote_h, &x, &y) < 0)
                    break;
                if (ev.type == SDL_FINGERDOWN)
                    flags |= SD_TOUCH_DOWN;
                else if (ev.type == SDL_FINGERUP)
                    flags |= SD_TOUCH_UP;
                sd_build_touch(pkt, x, y, (uint32_t)a.remote_w, (uint32_t)a.remote_h, flags,
                               (uint32_t)(ev.tfinger.fingerId & 0x7), now_ms());
                send_packet(&a, pkt, sizeof(pkt));
                break;
            }
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                if (!handle_hotkey(win, &ev.key))
                    handle_key(&a, &ev.key);
                break;
            default:
                break;
            }
        } while (!done && SDL_PollEvent(&ev));

        if (redraw) {
            SDL_LockMutex(a.slot.lock);
            if (a.slot.fresh) {
                tex = upload(&a, ren, tex, &a.slot.images[a.slot.front]);
                a.slot.fresh = 0;
            }
            SDL_UnlockMutex(a.slot.lock);
            SDL_RenderClear(ren);
            if (tex)
                SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
        }

        if (a.opt.verbose && SDL_GetTicks() - stats_t0 >= 1000) {
            int frames = SDL_AtomicSet(&a.frames_rx, 0);
            int bytes = SDL_AtomicSet(&a.bytes_rx, 0);
            fprintf(stderr, "stats: %d fps, %.1f Mbit/s, h264=%s\n", frames,
                    bytes * 8.0 / 1e6 / ((SDL_GetTicks() - stats_t0) / 1000.0),
                    sdv_decoder_h264_backend(a.dec));
            stats_t0 = SDL_GetTicks();
        }
    }

    SDL_AtomicSet(&a.running, 0);
    sd_build_disconnect(bye);
    sdv_conn_send(&a.conn, bye, sizeof(bye));
    sdv_conn_shutdown(&a.conn);
    SDL_WaitThread(thr, NULL);
    sdv_conn_close(&a.conn);

    sdv_decoder_free(a.dec);
    sdv_image_free(&a.slot.images[0]);
    sdv_image_free(&a.slot.images[1]);
    SDL_DestroyMutex(a.slot.lock);
    if (tex)
        SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
