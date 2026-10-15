/* SpaceDesk Linux: native viewer for spacedesk servers. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL.h>

#include "app.h"
#include "keymap.h"
#include "nk.h"
#include "protocol.h"

#define RECONNECT_DELAY_MS 3000
#define TOAST_MS 4500

static int verbose;

/* ---------- identity ---------- */

/* One stable GUID per installation: the server keys its per-device settings on it. */
static void load_device_id(char *out, size_t n)
{
    char path[600];
    unsigned char r[16];
    FILE *f;
    int fd;

    snprintf(path, sizeof(path), "%s/device-id", sdv_settings_dir());
    f = fopen(path, "r");
    if (f) {
        int ok = fgets(out, (int)n, f) != NULL;
        fclose(f);
        out[strcspn(out, "\r\n")] = 0;
        if (ok && out[0] == '{')
            return;
    }
    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0 || read(fd, r, sizeof(r)) != (ssize_t)sizeof(r))
        for (size_t i = 0; i < sizeof(r); i++)
            r[i] = (unsigned char)(rand() ^ (int)SDL_GetPerformanceCounter());
    if (fd >= 0)
        close(fd);
    r[6] = (r[6] & 0x0F) | 0x40;
    r[8] = (r[8] & 0x3F) | 0x80;
    snprintf(out, n, "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
    mkdir(sdv_settings_dir(), 0755);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", out);
        fclose(f);
    }
}

/* ---------- small helpers ---------- */

void app_toast(struct sdv_app *a, const char *msg)
{
    snprintf(a->toast, sizeof(a->toast), "%s", msg);
    a->toast_until = SDL_GetTicks() + TOAST_MS;
}

static void send_packet(struct sdv_app *a, const uint8_t *pkt, size_t len)
{
    if (a->sess)
        sdv_session_send(a->sess, pkt, len);
}

static int to_remote(const struct sdv_app *a, int wx, int wy, int *rx, int *ry)
{
    const SDL_Rect *v = &a->video;

    if (a->remote_w <= 0 || v->w <= 0 || v->h <= 0)
        return -1;
    if (wx < v->x || wy < v->y || wx >= v->x + v->w || wy >= v->y + v->h)
        return -1;
    *rx = (int)((int64_t)(wx - v->x) * a->remote_w / v->w);
    *ry = (int)((int64_t)(wy - v->y) * a->remote_h / v->h);
    return 0;
}

static void update_title(struct sdv_app *a)
{
    char title[256];
    if (a->streaming)
        snprintf(title, sizeof(title), "SpaceDesk Linux - %s", a->host);
    else
        snprintf(title, sizeof(title), "SpaceDesk Linux");
    SDL_SetWindowTitle(a->win, title);
}

static void update_screensaver(struct sdv_app *a)
{
    if (a->streaming && a->set.keep_monitor_active)
        SDL_DisableScreenSaver();
    else
        SDL_EnableScreenSaver();
}

/* Window icon: blue rounded tile with a white monitor outline, drawn at startup. */
static void set_window_icon(SDL_Window *win)
{
    enum { S = 64 };
    SDL_Surface *icon = SDL_CreateRGBSurfaceWithFormat(0, S, S, 32, SDL_PIXELFORMAT_ARGB8888);
    Uint32 *px;

    if (!icon)
        return;
    px = icon->pixels;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            int dx = x < 12 ? 12 - x : x > S - 13 ? x - (S - 13) : 0;
            int dy = y < 12 ? 12 - y : y > S - 13 ? y - (S - 13) : 0;
            int inside = dx * dx + dy * dy <= 144;
            int screen = x >= 14 && x <= 49 && y >= 16 && y <= 40;
            int screen_in = x >= 18 && x <= 45 && y >= 20 && y <= 36;
            int stand = (x >= 29 && x <= 34 && y > 40 && y <= 46) || (x >= 22 && x <= 41 && y >= 46 && y <= 49);
            Uint32 c = 0x00000000;
            if (inside)
                c = 0xFF2563EB + (Uint32)((y * 0x1A) / S) * 0x10000; /* blue, slightly lighter at the bottom */
            if (inside && ((screen && !screen_in) || stand))
                c = 0xFFFFFFFF;
            px[y * (icon->pitch / 4) + x] = c;
        }
    SDL_SetWindowIcon(win, icon);
    SDL_FreeSurface(icon);
}

/* Mouse-as-touch: Windows hides its own pointer for touch input, so keep a clear local one. */
static void update_cursor(struct sdv_app *a)
{
    static SDL_Cursor *arrow, *hand;
    int want_hand = a->streaming && a->set.mouse_mode == SDV_MOUSE_LOCAL_TOUCH && !ui_wants_input(a);

    if (!arrow) {
        arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    }
    if (!a->mouse_captured) {
        SDL_SetCursor(want_hand && hand ? hand : arrow);
        SDL_ShowCursor(SDL_ENABLE);
    }
}

/* ---------- discovery ---------- */

static void discovery_start(struct sdv_app *a)
{
    const char *extra[SDV_MAX_RECENT + 1] = { 0 };
    int n = 0;

    if (a->disc)
        return;
    for (int i = 0; i < SDV_MAX_RECENT; i++)
        if (a->set.recent[i][0])
            extra[n++] = a->set.recent[i];
    a->disc = sdv_discovery_start(extra);
}

static void discovery_stop(struct sdv_app *a)
{
    sdv_discovery_stop(a->disc);
    a->disc = NULL;
}

/* ---------- mouse capture ---------- */

static void capture_mouse(struct sdv_app *a)
{
    if (a->mouse_captured)
        return;
    if (SDL_SetRelativeMouseMode(SDL_TRUE) == 0) {
        a->mouse_captured = 1;
        app_toast(a, "Mouse capturado. Ctrl+Alt+M libera el mouse y abre el menú.");
    }
}

void app_release_mouse(struct sdv_app *a)
{
    if (a->touch_down) {
        uint8_t pkt[SD_HEADER_LEN];
        sd_build_touch(pkt, 0, 0, (uint32_t)a->remote_w, (uint32_t)a->remote_h,
                       SD_TOUCH_ABSOLUTE | SD_TOUCH_UP, SDL_GetTicks());
        send_packet(a, pkt, sizeof(pkt));
        a->touch_down = 0;
    }
    if (!a->mouse_captured)
        return;
    SDL_SetRelativeMouseMode(SDL_FALSE);
    a->mouse_captured = 0;
}

/* ---------- sessions ---------- */

static void build_session_config(struct sdv_app *a, struct sdv_session_config *c)
{
    struct sdv_settings *s = &a->set;
    int w, h;

    memset(c, 0, sizeof(*c));
    snprintf(c->host, sizeof(c->host), "%s", a->host);
    c->port = SD_PORT;
    snprintf(c->device_id, sizeof(c->device_id), "%s", a->device_id);
    snprintf(c->name, sizeof(c->name), "%s", a->name);
    c->allow_hw = s->hw_decode && !a->force_no_hw;

    if (s->custom_resolution) {
        w = s->custom_width;
        h = s->custom_height;
    } else {
        SDL_DisplayMode m;
        int idx = SDL_GetWindowDisplayIndex(a->win);
        if (idx >= 0 && SDL_GetDesktopDisplayMode(idx, &m) == 0) {
            w = m.w;
            h = m.h;
        } else {
            w = 1280;
            h = 720;
        }
    }
    if (s->turbo) {
        w = (w / 2) & ~1;
        h = (h / 2) & ~1;
    }
    c->ident.os_type = SD_OS_TYPE_VIEWER;
    c->ident.quality = (uint32_t)s->quality;
    /* The server paces capture at the requested rate plus its own overhead: asking for
     * 60 delivers about 45 fps, asking for 120 delivers a steady 60 (measured). Request
     * twice the setting; frames still only come when the screen changes. */
    c->ident.fps = (uint16_t)(s->fps * 2 > 120 ? 120 : s->fps * 2);
    c->ident.nres = 1;
    c->ident.res_w[0] = (uint32_t)w;
    c->ident.res_h[0] = (uint32_t)h;
    if (s->yuv)
        c->ident.encoding = SD_ENC_YUV420;
    else
        switch (s->encoding) {
        case SDV_ENC_PREF_H264: c->ident.encoding = SD_ENC_H264; break;
        case SDV_ENC_PREF_JPEG: c->ident.encoding = SD_ENC_JPEG; break;
        case SDV_ENC_PREF_OFF: c->ident.encoding = SD_ENC_BGRX; break;
        default: c->ident.encoding = SD_ENC_AUTO; break;
        }
}

static void stop_sessions(struct sdv_app *a)
{
    app_release_mouse(a);
    sdv_session_stop(a->sess);
    a->sess = NULL;
    sdv_audio_stop(a->audio);
    a->audio = NULL;
    if (a->tex)
        SDL_DestroyTexture(a->tex);
    a->tex = NULL;
    a->remote_w = a->remote_h = 0;
    a->streaming = 0;
}

void app_connect(struct sdv_app *a, const char *host)
{
    struct sdv_session_config cfg;

    if (!host || !*host)
        return;
    if (host != a->host)
        snprintf(a->host, sizeof(a->host), "%s", host);
    stop_sessions(a);
    discovery_stop(a);
    a->reconnect_at = 0;
    a->menu_open = 0;
    snprintf(a->message, sizeof(a->message), "Conectando a %s...", a->host);

    if (a->set.display_enabled) {
        build_session_config(a, &cfg);
        if (verbose)
            fprintf(stderr, "connect %s: %ux%u@%u encoding %u quality %u\n", a->host, cfg.ident.res_w[0],
                    cfg.ident.res_h[0], cfg.ident.fps, cfg.ident.encoding, cfg.ident.quality);
        a->sess = sdv_session_start(&cfg, a->ev_frame, a->ev_state);
    }
    if (a->set.audio_enabled)
        a->audio = sdv_audio_start(a->host, SD_PORT, a->device_id, a->name);
    if (!a->sess && !a->audio) {
        snprintf(a->message, sizeof(a->message), "Activa Pantalla o Audio en el menú Funciones para conectar.");
        discovery_start(a);
    }
    a->stats_t0 = SDL_GetTicks();
    a->stats_rx = a->stats_tx = 0;
    a->stats_frames = 0;
    update_title(a);
}

void app_disconnect(struct sdv_app *a, const char *message)
{
    stop_sessions(a);
    a->host[0] = 0;
    a->reconnect_at = 0;
    snprintf(a->message, sizeof(a->message), "%s", message ? message : "");
    if (a->fullscreen)
        app_set_fullscreen(a, 0);
    discovery_start(a);
    update_title(a);
    update_screensaver(a);
}

void app_apply_settings(struct sdv_app *a, int reconnect)
{
    if (sdv_settings_save(&a->set) < 0)
        app_toast(a, "No se pudieron guardar los ajustes");
    update_screensaver(a);
    if (a->set.mouse_mode != SDV_MOUSE_REMOTE)
        app_release_mouse(a);
    if (reconnect && a->host[0] && (a->sess || a->audio || a->streaming))
        app_connect(a, a->host);
}

void app_set_fullscreen(struct sdv_app *a, int on)
{
    if (SDL_SetWindowFullscreen(a->win, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0) {
        a->fullscreen = on;
        a->menu_open = 0;
        if (on)
            app_toast(a, "Pantalla completa. Ctrl+Alt+M muestra el menú, Alt+Enter sale.");
    }
}

/* Polls the display/audio sessions and reacts to state changes. */
static void check_sessions(struct sdv_app *a)
{
    char err[192] = "";

    if (a->sess) {
        enum sdv_session_state st = sdv_session_state(a->sess, err, sizeof(err));
        if (st == SDV_STATE_STREAMING && !a->streaming) {
            a->streaming = 1;
            a->message[0] = 0;
            sdv_settings_remember(&a->set, a->host);
            sdv_settings_save(&a->set);
            if (a->set.fullscreen_on_connect && !a->force_windowed && !a->fullscreen)
                app_set_fullscreen(a, 1);
            if (a->set.mouse_mode == SDV_MOUSE_REMOTE)
                app_toast(a, "Conectado. Haz clic en la pantalla para usar el mouse; Ctrl+Alt+M abre el menú.");
            update_title(a);
            update_screensaver(a);
        } else if (st == SDV_STATE_FAILED || st == SDV_STATE_CLOSED) {
            char host[128];
            int retry = a->set.autoconnect == SDV_AUTOCONNECT_ADDRESS || st == SDV_STATE_CLOSED;
            snprintf(host, sizeof(host), "%s", a->host);
            app_disconnect(a, err[0] ? err : "Desconectado");
            if (retry) {
                /* keep trying like the official viewer does after a power/network event */
                snprintf(a->host, sizeof(a->host), "%s", host);
                a->reconnect_at = SDL_GetTicks() + RECONNECT_DELAY_MS;
                snprintf(a->message + strlen(a->message), sizeof(a->message) - strlen(a->message),
                         " - reintentando...");
            }
        }
    } else if (a->audio && !a->set.display_enabled) {
        int st = sdv_audio_status(a->audio, err, sizeof(err));
        if (st == 1 && !a->streaming) {
            a->streaming = 1;
            a->message[0] = 0;
            update_title(a);
        }
    }
    if (a->audio) {
        int st = sdv_audio_status(a->audio, err, sizeof(err));
        if (st < 0) {
            app_toast(a, err);
            sdv_audio_stop(a->audio);
            a->audio = NULL;
        }
    }
    if (a->reconnect_at && SDL_TICKS_PASSED(SDL_GetTicks(), a->reconnect_at) && a->host[0])
        app_connect(a, a->host);
}

static void upload_frame(void *user, const struct sdv_image *img)
{
    static const Uint32 sdl_fmt[] = {
        [SDV_FMT_BGRX] = SDL_PIXELFORMAT_RGB888,
        [SDV_FMT_NV12] = SDL_PIXELFORMAT_NV12,
        [SDV_FMT_I420] = SDL_PIXELFORMAT_IYUV,
    };
    struct sdv_app *a = user;
    Uint32 fmt = 0;
    int tw = 0, th = 0;

    if (img->fmt == SDV_FMT_NONE)
        return;
    if (a->tex)
        SDL_QueryTexture(a->tex, &fmt, NULL, &tw, &th);
    if (!a->tex || fmt != sdl_fmt[img->fmt] || tw != img->width || th != img->height) {
        if (a->tex)
            SDL_DestroyTexture(a->tex);
        a->tex = SDL_CreateTexture(a->ren, sdl_fmt[img->fmt], SDL_TEXTUREACCESS_STREAMING, img->width, img->height);
        if (!a->tex) {
            fprintf(stderr, "render: %s\n", SDL_GetError());
            return;
        }
    }
    a->remote_w = img->src_w;
    a->remote_h = img->src_h;
    Uint64 tu = SDL_GetPerformanceCounter();
    switch (img->fmt) {
    case SDV_FMT_BGRX:
        SDL_UpdateTexture(a->tex, NULL, img->plane[0], img->pitch[0]);
        break;
    case SDV_FMT_NV12:
        SDL_UpdateNVTexture(a->tex, NULL, img->plane[0], img->pitch[0], img->plane[1], img->pitch[1]);
        break;
    case SDV_FMT_I420:
        SDL_UpdateYUVTexture(a->tex, NULL, img->plane[0], img->pitch[0], img->plane[1], img->pitch[1],
                             img->plane[2], img->pitch[2]);
        break;
    default:
        break;
    }
    a->prof_upload_us += (SDL_GetPerformanceCounter() - tu) * 1000000 / SDL_GetPerformanceFrequency();
    a->prof_uploads++;
}

static void update_stats(struct sdv_app *a)
{
    struct sdv_session_stats st;
    Uint32 now = SDL_GetTicks(), dt = now - a->stats_t0;

    if (!a->sess || dt < 1000)
        return;
    sdv_session_get_stats(a->sess, &st);
    a->fps = (st.frames - a->stats_frames) * 1000.0 / dt;
    a->mbps = (st.bytes_rx - a->stats_rx) * 8.0 / 1000.0 / dt;
    if (verbose) {
        unsigned n = st.frames - a->stats_frames;
        double per = n ? 1000.0 * n : 1.0; /* accumulated us -> ms per frame */
        fprintf(stderr,
                "stats: %.0f fps %.2f Mbit/s %s %s %dx%d | wait %.1f dec %.1f xfer %.1f upload %.1f ms/frame, max %u KiB\n",
                a->fps, a->mbps, sd_encoding_name(st.encoding), st.h264_backend ? st.h264_backend : "", st.remote_w,
                st.remote_h, (st.t_wait_us - a->prof_wait) / per, (st.t_decode_us - a->prof_decode) / per,
                (st.t_xfer_us - a->prof_xfer) / per,
                a->prof_upload_us / (a->prof_uploads ? 1000.0 * a->prof_uploads : 1.0), st.max_frame_bytes / 1024);
        a->prof_wait = st.t_wait_us;
        a->prof_decode = st.t_decode_us;
        a->prof_xfer = st.t_xfer_us;
        a->prof_upload_us = 0;
        a->prof_uploads = 0;
    }
    a->stats_frames = st.frames;
    a->stats_rx = st.bytes_rx;
    a->stats_tx = st.bytes_tx;
    a->stats_t0 = now;
}

/* ---------- input ---------- */

static uint32_t button_flags(Uint8 button, int down)
{
    switch (button) {
    case SDL_BUTTON_LEFT: return down ? SD_MOUSE_LEFTDOWN : SD_MOUSE_LEFTUP;
    case SDL_BUTTON_RIGHT: return down ? SD_MOUSE_RIGHTDOWN : SD_MOUSE_RIGHTUP;
    case SDL_BUTTON_MIDDLE: return down ? SD_MOUSE_MIDDLEDOWN : SD_MOUSE_MIDDLEUP;
    default: return 0;
    }
}

static void send_mouse(struct sdv_app *a, int dx, int dy, int wheel, uint32_t flags)
{
    uint8_t pkt[SD_HEADER_LEN];
    sd_build_mouse(pkt, dx, dy, wheel, flags);
    send_packet(a, pkt, sizeof(pkt));
}

static void send_touch(struct sdv_app *a, int x, int y, uint32_t flags)
{
    uint8_t pkt[SD_HEADER_LEN];
    sd_build_touch(pkt, x, y, (uint32_t)a->remote_w, (uint32_t)a->remote_h, flags, SDL_GetTicks());
    send_packet(a, pkt, sizeof(pkt));
}

static void send_key(struct sdv_app *a, const SDL_KeyboardEvent *k)
{
    struct sdv_key key;
    uint8_t pkt[SD_HEADER_LEN];
    uint32_t flags = 0;

    if (sdv_keymap_lookup(k->keysym.scancode, &key) < 0)
        return;
    if (key.extended)
        flags |= SD_KEY_EXTENDED;
    if (k->type == SDL_KEYUP)
        flags |= SD_KEY_UP;
    sd_build_keyboard(pkt, key.vk, key.scan, flags);
    send_packet(a, pkt, sizeof(pkt));
}

/* Modifiers pressed before a local hotkey or a focus change already reached Windows;
 * release them there so they do not stay stuck. */
static void release_modifiers(struct sdv_app *a)
{
    static const SDL_Scancode mods[] = {
        SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL, SDL_SCANCODE_LALT, SDL_SCANCODE_RALT,
        SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI,
    };
    SDL_KeyboardEvent k = { .type = SDL_KEYUP };

    if (!a->sess || !a->streaming)
        return;
    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
        k.keysym.scancode = mods[i];
        send_key(a, &k);
    }
}

/* Local hotkeys; returns 1 when the key was consumed. */
static int hotkey(struct sdv_app *a, const SDL_KeyboardEvent *k)
{
    SDL_Keymod mod = SDL_GetModState();

    if (k->type != SDL_KEYDOWN || k->repeat)
        return k->type == SDL_KEYDOWN && k->repeat && (mod & KMOD_CTRL) && (mod & KMOD_ALT) &&
               k->keysym.scancode == SDL_SCANCODE_M;
    if ((mod & KMOD_ALT) && (k->keysym.scancode == SDL_SCANCODE_RETURN || k->keysym.scancode == SDL_SCANCODE_KP_ENTER)) {
        release_modifiers(a);
        app_set_fullscreen(a, !a->fullscreen);
        return 1;
    }
    if ((mod & KMOD_CTRL) && (mod & KMOD_ALT) && k->keysym.scancode == SDL_SCANCODE_M) {
        release_modifiers(a);
        app_release_mouse(a);
        if (a->fullscreen) {
            a->menu_open = !a->menu_open;
            a->menu_until = SDL_GetTicks() + 3000;
        }
        return 1;
    }
    if (a->set.mouse_mode == SDV_MOUSE_LOCAL_TOUCH && (mod & KMOD_ALT) && (mod & KMOD_SHIFT) &&
        (k->keysym.scancode == SDL_SCANCODE_LSHIFT || k->keysym.scancode == SDL_SCANCODE_RSHIFT ||
         k->keysym.scancode == SDL_SCANCODE_LALT || k->keysym.scancode == SDL_SCANCODE_RALT)) {
        app_release_mouse(a);
        a->set.mouse_mode = SDV_MOUSE_REMOTE;
        app_apply_settings(a, 0);
        app_toast(a, "Emulación táctil desactivada: modo Mouse remoto.");
        return 0; /* the modifier itself still reaches Windows */
    }
    return 0;
}

static void handle_event(struct sdv_app *a, SDL_Event *ev)
{
    int x, y, live = a->streaming && a->sess && a->remote_w > 0;
    int ui = ui_wants_input(a);

    switch (ev->type) {
    case SDL_QUIT:
        a->quit = 1;
        return;
    case SDL_WINDOWEVENT:
        if (ev->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            release_modifiers(a);
            app_release_mouse(a);
        }
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if (hotkey(a, &ev->key))
            return;
        if (live && !ui && a->set.keyboard_enabled) {
            send_key(a, &ev->key);
            return;
        }
        break;
    case SDL_TEXTINPUT:
        if (live && !ui)
            return;
        break;
    case SDL_MOUSEMOTION:
        if (a->mouse_captured) {
            if (live)
                send_mouse(a, ev->motion.xrel, ev->motion.yrel, 0, 0);
            return;
        }
        if (a->fullscreen && !a->menu_open && ev->motion.y <= 1 && ev->motion.which != SDL_TOUCH_MOUSEID) {
            a->menu_open = 1;
            a->menu_until = SDL_GetTicks() + 1000;
        }
        if (live && !ui && a->touch_down && to_remote(a, ev->motion.x, ev->motion.y, &x, &y) == 0)
            send_touch(a, x, y, SD_TOUCH_ABSOLUTE);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        int down = ev->type == SDL_MOUSEBUTTONDOWN;
        if (a->mouse_captured) {
            if (live && button_flags(ev->button.button, down))
                send_mouse(a, 0, 0, 0, button_flags(ev->button.button, down));
            return;
        }
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            break; /* touchscreen taps are handled as fingers; keep them for the UI only */
        if (!live || ui)
            break;
        if (to_remote(a, ev->button.x, ev->button.y, &x, &y) < 0) {
            if (a->menu_open && down)
                a->menu_open = 0;
            break;
        }
        if (a->menu_open && down) {
            a->menu_open = 0;
            return;
        }
        if (a->set.mouse_mode == SDV_MOUSE_REMOTE) {
            if (down) {
                capture_mouse(a);
                if (button_flags(ev->button.button, 1))
                    send_mouse(a, 0, 0, 0, button_flags(ev->button.button, 1));
            }
            return;
        }
        /* mouse local: the left button drives one touch contact */
        if (ev->button.button == SDL_BUTTON_LEFT) {
            send_touch(a, x, y, SD_TOUCH_ABSOLUTE | (down ? SD_TOUCH_DOWN : SD_TOUCH_UP));
            a->touch_down = down;
        }
        return;
    }
    case SDL_MOUSEWHEEL:
        if (a->mouse_captured || (live && !ui)) {
            if (live && ev->wheel.y)
                send_mouse(a, 0, 0, ev->wheel.y * SD_WHEEL_DELTA, SD_MOUSE_WHEEL);
            if (live && ev->wheel.x)
                send_mouse(a, 0, 0, ev->wheel.x * SD_WHEEL_DELTA, SD_MOUSE_HWHEEL);
            return;
        }
        break;
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
    case SDL_FINGERUP: {
        int ww, wh;
        uint32_t flags = a->set.touch_mode == SDV_TOUCH_ABSOLUTE ? SD_TOUCH_ABSOLUTE : 0;
        if (!live || ui || a->set.touch_mode == SDV_TOUCH_OFF)
            break;
        SDL_GetWindowSize(a->win, &ww, &wh);
        if (to_remote(a, (int)(ev->tfinger.x * ww), (int)(ev->tfinger.y * wh), &x, &y) < 0)
            break;
        if (ev->type == SDL_FINGERDOWN)
            flags |= SD_TOUCH_DOWN;
        else if (ev->type == SDL_FINGERUP)
            flags |= SD_TOUCH_UP;
        send_touch(a, x, y, flags);
        return;
    }
    default:
        break;
    }
    nk_sdl_handle_event(ev);
}

/* ---------- main ---------- */

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [options] [server]\n"
            "  -w, --windowed   start in a window even if 'full screen upon connect' is set\n"
            "      --no-hw      disable VA-API decoding for this run\n"
            "  -v, --verbose    log connection details and per-frame timing every second\n"
            "      --version    print the version and exit\n"
            "Hotkeys: Alt+Enter full screen, Ctrl+Alt+M menu / release mouse\n",
            argv0);
}

int main(int argc, char **argv)
{
    static const struct option longopts[] = {
        { "windowed", no_argument, NULL, 'w' },
        { "no-hw", no_argument, NULL, 'H' },
        { "verbose", no_argument, NULL, 'v' },
        { "help", no_argument, NULL, 'h' },
        { "version", no_argument, NULL, 'V' },
        { 0 },
    };
    static struct sdv_app app;
    struct sdv_app *a = &app;
    const char *cli_host = NULL;
    int windowed = 0, no_hw = 0, c;
    char hostname[64];

    while ((c = getopt_long(argc, argv, "wvh", longopts, NULL)) != -1) {
        switch (c) {
        case 'w': windowed = 1; break;
        case 'H': no_hw = 1; break;
        case 'v': verbose = 1; break;
        case 'V': printf("sdviewer %s\n", SDV_VERSION); return 0;
        default: usage(argv[0]); return c == 'h' ? 0 : 2;
        }
    }
    if (optind < argc)
        cli_host = argv[optind];

    sdv_settings_load(&a->set);
    a->force_no_hw = no_hw;
    a->force_windowed = windowed;
    load_device_id(a->device_id, sizeof(a->device_id));
    if (gethostname(hostname, sizeof(hostname)) != 0)
        snprintf(hostname, sizeof(hostname), "linux");
    hostname[sizeof(hostname) - 1] = 0;
    snprintf(a->name, sizeof(a->name), "%s", hostname);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_APP_NAME, "SpaceDesk Linux");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    a->win = SDL_CreateWindow("SpaceDesk Linux", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1100, 700,
                              SDL_WINDOW_RESIZABLE);
    if (!a->win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    set_window_icon(a->win);
    SDL_SetWindowMinimumSize(a->win, 640, 480);
    a->ren = SDL_CreateRenderer(a->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!a->ren)
        a->ren = SDL_CreateRenderer(a->win, -1, 0);
    if (!a->ren || ui_init(a) < 0) {
        fprintf(stderr, "renderer: %s\n", SDL_GetError());
        return 1;
    }
    a->ev_frame = SDL_RegisterEvents(2);
    a->ev_state = a->ev_frame + 1;
    if (verbose) {
        SDL_RendererInfo ri;
        SDL_GetRendererInfo(a->ren, &ri);
        fprintf(stderr, "video driver %s, renderer %s, device id %s\n", SDL_GetCurrentVideoDriver(), ri.name,
                a->device_id);
    }

    if (cli_host)
        app_connect(a, cli_host);
    else if (a->set.autoconnect == SDV_AUTOCONNECT_ADDRESS && a->set.server[0])
        app_connect(a, a->set.server);
    else
        discovery_start(a);
    update_screensaver(a);

    /* debugging aid: SDV_OPEN_DIALOG opens a settings dialog at startup (for screenshots) */
    if (getenv("SDV_OPEN_DIALOG")) {
        static const char *names[] = { "", "resolutions", "quality", "autoconnect", "advanced", "about" };
        for (int i = 1; i < (int)(sizeof(names) / sizeof(names[0])); i++)
            if (!strcmp(getenv("SDV_OPEN_DIALOG"), names[i])) {
                a->edit = a->set;
                a->dialog = (enum sdv_dialog)i;
            }
    }

    /* debugging aid: SDV_SNAPSHOT=/path/file.bmp saves one rendered frame after a few seconds */
    const char *snapshot = getenv("SDV_SNAPSHOT");
    Uint32 snapshot_at = SDL_GetTicks() + (getenv("SDV_SNAPSHOT_MS") ? (Uint32)atoi(getenv("SDV_SNAPSHOT_MS")) : 6000);

    while (!a->quit) {
        SDL_Event ev;
        nk_input_begin(a->nk);
        if (SDL_WaitEventTimeout(&ev, a->streaming ? 8 : 33)) {
            handle_event(a, &ev);
            while (SDL_PollEvent(&ev))
                handle_event(a, &ev);
        }
        nk_sdl_handle_grab();
        nk_input_end(a->nk);

        check_sessions(a);
        if (a->sess)
            sdv_session_take_frame(a->sess, upload_frame, a);
        update_stats(a);

        ui_frame(a);
        if (a->sess) {
            /* video rect is in window coordinates; the decoder wants output pixels */
            int ww, wh, ow, oh;
            SDL_GetWindowSize(a->win, &ww, &wh);
            SDL_GetRendererOutputSize(a->ren, &ow, &oh);
            if (ww > 0 && wh > 0)
                sdv_session_set_target(a->sess, a->video.w * ow / ww, a->video.h * oh / wh);
        }
        update_cursor(a);
        if (a->streaming)
            SDL_SetRenderDrawColor(a->ren, 0, 0, 0, 255);
        else
            SDL_SetRenderDrawColor(a->ren, 23, 24, 26, 255);
        SDL_RenderClear(a->ren);
        if (a->tex && a->streaming)
            SDL_RenderCopy(a->ren, a->tex, NULL, &a->video);
        nk_sdl_render(NK_ANTI_ALIASING_ON);
        if (snapshot && SDL_TICKS_PASSED(SDL_GetTicks(), snapshot_at)) {
            int w, h;
            SDL_Surface *surf;
            SDL_GetRendererOutputSize(a->ren, &w, &h);
            surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
            if (surf && SDL_RenderReadPixels(a->ren, NULL, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch) == 0)
                SDL_SaveBMP(surf, snapshot);
            SDL_FreeSurface(surf);
            snapshot = NULL;
        }
        SDL_RenderPresent(a->ren);
    }

    sdv_settings_save(&a->set);
    stop_sessions(a);
    discovery_stop(a);
    ui_shutdown();
    SDL_DestroyRenderer(a->ren);
    SDL_DestroyWindow(a->win);
    SDL_Quit();
    return 0;
}
