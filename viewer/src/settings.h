/* Persistent viewer settings (~/.config/sdviewer/settings.ini). */
#ifndef SDV_SETTINGS_H
#define SDV_SETTINGS_H

#define SDV_MAX_RECENT 8
#define SDV_HOST_LEN 128

enum sdv_mouse_mode {
    SDV_MOUSE_REMOTE = 0, /* relative mouse packets, pointer captured in the window */
    SDV_MOUSE_LOCAL_TOUCH, /* local pointer drives absolute touch contacts */
};

enum sdv_touch_mode {
    SDV_TOUCH_ABSOLUTE = 0, /* touchscreen */
    SDV_TOUCH_RELATIVE,     /* touchpad / trackpad */
    SDV_TOUCH_OFF,
};

enum sdv_encoding_pref {
    SDV_ENC_PREF_AUTO = 0, /* "choose best": H.264, JPEG fallback */
    SDV_ENC_PREF_H264,
    SDV_ENC_PREF_JPEG,
    SDV_ENC_PREF_OFF, /* uncompressed BGRX */
};

enum sdv_autoconnect {
    SDV_AUTOCONNECT_OFF = 0,
    SDV_AUTOCONNECT_ADDRESS,
};

struct sdv_settings {
    /* connection */
    char server[SDV_HOST_LEN];
    char recent[SDV_MAX_RECENT][SDV_HOST_LEN];
    int autoconnect;
    int fullscreen_on_connect;
    int keep_monitor_active;
    int allow_localhost;

    /* resolution */
    int custom_resolution; /* 0 = native screen resolution */
    int custom_width, custom_height;

    /* quality / performance */
    int encoding;
    int quality; /* 1..100, "quality vs speed" */
    int fps;     /* 30, 60, 90, 120 */
    int turbo;   /* request half resolution and upscale locally */

    /* advanced */
    int yuv;       /* uncompressed 4:2:0 instead of the chosen encoding */
    int hw_decode; /* VA-API H.264 decoding */

    /* functionality */
    int display_enabled;
    int keyboard_enabled;
    int mouse_mode;
    int touch_mode;
    int audio_enabled;

    /* view */
    int status_bar;
};

void sdv_settings_defaults(struct sdv_settings *s);
void sdv_settings_load(struct sdv_settings *s);
int sdv_settings_save(const struct sdv_settings *s);
/* Moves host to the front of the recent list. */
void sdv_settings_remember(struct sdv_settings *s, const char *host);
/* Directory holding settings.ini and device-id (no trailing slash). */
const char *sdv_settings_dir(void);

#endif
