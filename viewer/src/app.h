/* Application state shared by the main loop (main.c) and the user interface (ui.c). */
#ifndef SDV_APP_H
#define SDV_APP_H

#include <SDL.h>

#include "audio.h"
#include "decoder.h"
#include "discovery.h"
#include "session.h"
#include "settings.h"

#define SDV_VERSION "0.2.0"
#define SDV_MENU_H 26
#define SDV_STATUS_H 22

struct nk_context;

enum sdv_dialog {
    SDV_DLG_NONE = 0,
    SDV_DLG_RESOLUTIONS,
    SDV_DLG_QUALITY,
    SDV_DLG_AUTOCONNECT,
    SDV_DLG_ADVANCED,
    SDV_DLG_ABOUT,
};

struct sdv_app {
    SDL_Window *win;
    SDL_Renderer *ren;
    struct nk_context *nk;

    struct sdv_settings set;
    struct sdv_settings edit; /* working copy for the open dialog */
    char device_id[64];
    char name[128];
    Uint32 ev_frame, ev_state;

    /* connection */
    char host[128];
    struct sdv_session *sess;
    struct sdv_audio *audio;
    struct sdv_discovery *disc;
    int streaming;
    char message[192]; /* last connection error / info for the connect screen */
    Uint32 reconnect_at;

    /* video */
    SDL_Texture *tex;
    int remote_w, remote_h;
    SDL_Rect video; /* window area showing the remote desktop */

    /* view / input state */
    int fullscreen;
    int menu_open;      /* menu bar shown on top of the video in fullscreen */
    int mouse_captured; /* remote mouse: relative mode active */
    int touch_down;     /* local mouse emulating a touch contact */
    enum sdv_dialog dialog;
    char address[128]; /* connect screen input */
    char autoconnect_host[128];
    char toast[192];
    Uint32 toast_until;

    /* statistics for the status bar */
    Uint32 stats_t0;
    uint64_t stats_rx, stats_tx;
    unsigned stats_frames;
    double fps, mbps;
    int quit;

    /* command-line overrides for this run only (not saved) */
    int force_windowed;
    int force_no_hw;
};

/* actions implemented in main.c and triggered from the UI */
void app_connect(struct sdv_app *a, const char *host);
void app_disconnect(struct sdv_app *a, const char *message);
void app_apply_settings(struct sdv_app *a, int reconnect);
void app_set_fullscreen(struct sdv_app *a, int on);
void app_release_mouse(struct sdv_app *a);
void app_toast(struct sdv_app *a, const char *msg);

/* ui.c */
int ui_init(struct sdv_app *a);
void ui_shutdown(void);
/* Builds the whole UI for this frame and computes a->video. */
void ui_frame(struct sdv_app *a);
/* True when the UI wants keyboard/mouse input (menus or dialogs in use). */
int ui_wants_input(struct sdv_app *a);

#endif
