#include "settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *sdv_settings_dir(void)
{
    static char dir[512];
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (!dir[0]) {
        if (xdg && *xdg)
            snprintf(dir, sizeof(dir), "%s/sdviewer", xdg);
        else
            snprintf(dir, sizeof(dir), "%s/.config/sdviewer", home ? home : ".");
    }
    return dir;
}

void sdv_settings_defaults(struct sdv_settings *s)
{
    memset(s, 0, sizeof(*s));
    s->autoconnect = SDV_AUTOCONNECT_OFF;
    s->fullscreen_on_connect = 1;
    s->keep_monitor_active = 1;
    s->custom_width = 1920;
    s->custom_height = 1080;
    s->encoding = SDV_ENC_PREF_AUTO;
    s->quality = 70;
    s->fps = 60;
    s->hw_decode = 1;
    s->display_enabled = 1;
    s->keyboard_enabled = 1;
    s->mouse_mode = SDV_MOUSE_REMOTE;
    s->touch_mode = SDV_TOUCH_ABSOLUTE;
    s->status_bar = 0;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static void copy_str(char *dst, size_t n, const char *src)
{
    size_t len = strnlen(src, n - 1);
    memcpy(dst, src, len);
    dst[len] = 0;
}

#define INT_KEYS(X)                                   \
    X(autoconnect) X(fullscreen_on_connect)           \
    X(keep_monitor_active) X(allow_localhost)         \
    X(custom_resolution) X(custom_width)              \
    X(custom_height) X(encoding) X(quality) X(fps)    \
    X(turbo) X(yuv) X(hw_decode)                      \
    X(display_enabled) X(keyboard_enabled)            \
    X(mouse_mode) X(touch_mode) X(audio_enabled)      \
    X(status_bar)

void sdv_settings_load(struct sdv_settings *s)
{
    char path[600], line[512];
    FILE *f;
    int nrecent = 0;

    sdv_settings_defaults(s);
    snprintf(path, sizeof(path), "%s/settings.ini", sdv_settings_dir());
    f = fopen(path, "r");
    if (!f) {
        /* migrate the server address written by packaging/install-user.sh */
        snprintf(path, sizeof(path), "%s/server", sdv_settings_dir());
        f = fopen(path, "r");
        if (f) {
            if (fgets(line, sizeof(line), f)) {
                line[strcspn(line, "\r\n \t")] = 0;
                copy_str(s->server, sizeof(s->server), line);
                if (s->server[0]) {
                    sdv_settings_remember(s, s->server);
                    s->autoconnect = SDV_AUTOCONNECT_ADDRESS;
                }
            }
            fclose(f);
        }
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '='), *key = line, *val;
        if (!eq || line[0] == '#')
            continue;
        *eq = 0;
        val = eq + 1;
        val[strcspn(val, "\r\n")] = 0;
        if (!strcmp(key, "server"))
            copy_str(s->server, sizeof(s->server), val);
        else if (!strcmp(key, "recent") && nrecent < SDV_MAX_RECENT)
            copy_str(s->recent[nrecent++], SDV_HOST_LEN, val);
#define READ_INT(name) else if (!strcmp(key, #name)) s->name = atoi(val);
        INT_KEYS(READ_INT)
#undef READ_INT
    }
    fclose(f);

    s->quality = clampi(s->quality, 1, 100);
    if (s->fps != 30 && s->fps != 60 && s->fps != 90 && s->fps != 120)
        s->fps = 60;
    s->custom_width = clampi(s->custom_width, 320, 7680);
    s->custom_height = clampi(s->custom_height, 240, 4320);
    s->encoding = clampi(s->encoding, SDV_ENC_PREF_AUTO, SDV_ENC_PREF_OFF);
    s->mouse_mode = clampi(s->mouse_mode, SDV_MOUSE_REMOTE, SDV_MOUSE_LOCAL_TOUCH);
    s->touch_mode = clampi(s->touch_mode, SDV_TOUCH_ABSOLUTE, SDV_TOUCH_OFF);
}

int sdv_settings_save(const struct sdv_settings *s)
{
    char path[600], tmp[620];
    FILE *f;

    mkdir(sdv_settings_dir(), 0755);
    snprintf(path, sizeof(path), "%s/settings.ini", sdv_settings_dir());
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "w");
    if (!f)
        return -1;
    fprintf(f, "# sdviewer settings\nserver=%s\n", s->server);
    for (int i = 0; i < SDV_MAX_RECENT; i++)
        if (s->recent[i][0])
            fprintf(f, "recent=%s\n", s->recent[i]);
#define WRITE_INT(name) fprintf(f, #name "=%d\n", s->name);
    INT_KEYS(WRITE_INT)
#undef WRITE_INT
    if (fclose(f) != 0)
        return -1;
    return rename(tmp, path);
}

void sdv_settings_remember(struct sdv_settings *s, const char *host)
{
    char keep[SDV_MAX_RECENT][SDV_HOST_LEN];
    int n = 0;

    if (!host || !*host)
        return;
    copy_str(keep[n++], SDV_HOST_LEN, host);
    for (int i = 0; i < SDV_MAX_RECENT && n < SDV_MAX_RECENT; i++)
        if (s->recent[i][0] && strcmp(s->recent[i], host))
            copy_str(keep[n++], SDV_HOST_LEN, s->recent[i]);
    memset(s->recent, 0, sizeof(s->recent));
    memcpy(s->recent, keep, sizeof(keep[0]) * (size_t)n);
}
