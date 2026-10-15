/* User interface: menu bar, connect screen, settings dialogs and status bar (Nuklear). */
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "nk.h"
#include "protocol.h"

#define ROW 24
#define PROJECT_URL "https://github.com/MarcozVD/SpaceDeskLinux"
#define MANUAL_URL "https://www.spacedesk.net/user-manual/"

static const char *font_candidates[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/opentype/cantarell/Cantarell-Regular.otf",
    NULL,
};

int ui_init(struct sdv_app *a)
{
    struct nk_font_atlas *atlas;
    struct nk_font *font = NULL;

    a->nk = nk_sdl_init(a->win, a->ren);
    if (!a->nk)
        return -1;
    nk_sdl_font_stash_begin(&atlas);
    for (int i = 0; font_candidates[i] && !font; i++) {
        FILE *f = fopen(font_candidates[i], "rb");
        if (f) {
            fclose(f);
            font = nk_font_atlas_add_from_file(atlas, font_candidates[i], 15, NULL);
        }
    }
    if (!font)
        font = nk_font_atlas_add_default(atlas, 14, NULL);
    nk_sdl_font_stash_end();
    nk_style_set_font(a->nk, &font->handle);
    return 0;
}

void ui_shutdown(void)
{
    nk_sdl_shutdown();
}

int ui_wants_input(struct sdv_app *a)
{
    return !a->streaming || a->dialog != SDV_DLG_NONE || (a->fullscreen && a->menu_open) ||
           nk_window_is_any_hovered(a->nk) || nk_item_is_any_active(a->nk);
}

/* ---------- helpers ---------- */

static void separator(struct nk_context *ctx)
{
    nk_layout_row_dynamic(ctx, 7, 1);
    nk_rule_horizontal(ctx, nk_rgb(80, 80, 80), nk_false);
    nk_layout_row_dynamic(ctx, ROW, 1);
}

static int check(struct nk_context *ctx, const char *label, int *value)
{
    nk_bool v = *value ? nk_true : nk_false;
    int changed = nk_checkbox_label(ctx, label, &v);
    *value = v ? 1 : 0;
    return changed;
}

static int option(struct nk_context *ctx, const char *label, int *value, int choice)
{
    if (nk_option_label(ctx, label, *value == choice) && *value != choice) {
        *value = choice;
        return 1;
    }
    return 0;
}

static struct nk_rect centered(struct sdv_app *a, float w, float h)
{
    int ww, wh;
    SDL_GetWindowSize(a->win, &ww, &wh);
    if (w > ww - 20)
        w = (float)ww - 20;
    if (h > wh - 20)
        h = (float)wh - 20;
    return nk_rect((ww - w) / 2, (wh - h) / 2, w, h);
}

static void open_dialog(struct sdv_app *a, enum sdv_dialog d)
{
    a->edit = a->set;
    if (d == SDV_DLG_AUTOCONNECT)
        snprintf(a->autoconnect_host, sizeof(a->autoconnect_host), "%s",
                 a->set.server[0] ? a->set.server : a->host);
    a->dialog = d;
    a->menu_open = 0;
    app_release_mouse(a);
}

static void native_resolution(struct sdv_app *a, int *w, int *h)
{
    SDL_DisplayMode m;
    int idx = SDL_GetWindowDisplayIndex(a->win);
    if (idx < 0 || SDL_GetDesktopDisplayMode(idx, &m) < 0) {
        *w = 1280;
        *h = 720;
        return;
    }
    *w = m.w;
    *h = m.h;
}

static void format_bytes(char *out, size_t n, uint64_t b)
{
    if (b >= 1024ull * 1024 * 1024)
        snprintf(out, n, "%.2f GB", b / (1024.0 * 1024 * 1024));
    else if (b >= 1024 * 1024)
        snprintf(out, n, "%.1f MB", b / (1024.0 * 1024));
    else
        snprintf(out, n, "%.0f KB", b / 1024.0);
}

/* ---------- menu bar ---------- */

static void menu_bar(struct sdv_app *a, int width)
{
    struct nk_context *ctx = a->nk;
    struct sdv_settings *s = &a->set;

    if (!nk_begin(ctx, "menubar", nk_rect(0, 0, (float)width, SDV_MENU_H), NK_WINDOW_NO_SCROLLBAR)) {
        nk_end(ctx);
        return;
    }
    nk_menubar_begin(ctx);
    nk_layout_row_static(ctx, 18, 92, 5);

    if (nk_menu_begin_label(ctx, "Archivo", NK_TEXT_LEFT, nk_vec2(220, 120))) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (a->streaming || a->sess || a->audio) {
            if (nk_menu_item_label(ctx, "Desconectar", NK_TEXT_LEFT))
                app_disconnect(a, "Desconectado");
        } else {
            nk_label_colored(ctx, "Desconectar", NK_TEXT_LEFT, nk_rgb(110, 110, 110));
        }
        if (nk_menu_item_label(ctx, "Salir", NK_TEXT_LEFT))
            a->quit = 1;
        nk_menu_end(ctx);
    }

    if (nk_menu_begin_label(ctx, "Ver", NK_TEXT_LEFT, nk_vec2(280, 100))) {
        int fs = a->fullscreen;
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (check(ctx, "Barra de estado", &s->status_bar))
            app_apply_settings(a, 0);
        if (check(ctx, "Pantalla completa      Alt+Enter", &fs))
            app_set_fullscreen(a, fs);
        nk_menu_end(ctx);
    }

    if (nk_menu_begin_label(ctx, "Funciones", NK_TEXT_LEFT, nk_vec2(300, 320))) {
        int reconnect = 0, changed = 0;
        nk_layout_row_dynamic(ctx, ROW, 1);
        reconnect |= check(ctx, "Pantalla", &s->display_enabled);
        changed |= check(ctx, "Teclado", &s->keyboard_enabled);
        separator(ctx);
        if (option(ctx, "Mouse remoto", &s->mouse_mode, SDV_MOUSE_REMOTE)) {
            changed = 1;
            app_toast(a, "Mouse remoto: haz clic en la pantalla para capturarlo. Ctrl+Alt+M lo libera.");
        }
        if (option(ctx, "Mouse local (usar como táctil)", &s->mouse_mode, SDV_MOUSE_LOCAL_TOUCH)) {
            changed = 1;
            app_release_mouse(a);
            app_toast(a, "Emulación táctil con el mouse. Alt+Shift para salir.");
        }
        separator(ctx);
        changed |= option(ctx, "Pantalla táctil (absoluto)", &s->touch_mode, SDV_TOUCH_ABSOLUTE);
        changed |= option(ctx, "Touchpad / Trackpad (relativo)", &s->touch_mode, SDV_TOUCH_RELATIVE);
        changed |= option(ctx, "Táctil apagado", &s->touch_mode, SDV_TOUCH_OFF);
        separator(ctx);
        reconnect |= check(ctx, "Audio (altavoz)", &s->audio_enabled);
        if (reconnect || changed)
            app_apply_settings(a, reconnect);
        nk_menu_end(ctx);
    }

    if (nk_menu_begin_label(ctx, "Ajustes", NK_TEXT_LEFT, nk_vec2(300, 290))) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (nk_menu_item_label(ctx, "Resoluciones...", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_RESOLUTIONS);
        if (nk_menu_item_label(ctx, "Calidad / Rendimiento...", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_QUALITY);
        separator(ctx);
        if (nk_menu_item_label(ctx, "Conexión automática...", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_AUTOCONNECT);
        if (check(ctx, "Pantalla completa al conectar", &s->fullscreen_on_connect))
            app_apply_settings(a, 0);
        separator(ctx);
        if (check(ctx, "Mantener monitor activo", &s->keep_monitor_active))
            app_apply_settings(a, 0);
        separator(ctx);
        if (nk_menu_item_label(ctx, "Restablecer ajustes", NK_TEXT_LEFT)) {
            char recent[SDV_MAX_RECENT][SDV_HOST_LEN];
            memcpy(recent, s->recent, sizeof(recent));
            sdv_settings_defaults(s);
            memcpy(s->recent, recent, sizeof(recent));
            app_apply_settings(a, 1);
            app_toast(a, "Ajustes restablecidos");
        }
        separator(ctx);
        if (nk_menu_item_label(ctx, "Avanzado...", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_ADVANCED);
        nk_menu_end(ctx);
    }

    if (nk_menu_begin_label(ctx, "Ayuda", NK_TEXT_LEFT, nk_vec2(300, 110))) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (nk_menu_item_label(ctx, "Manual de usuario de spacedesk", NK_TEXT_LEFT))
            SDL_OpenURL(MANUAL_URL);
        if (nk_menu_item_label(ctx, "Proyecto SpaceDesk Linux (GitHub)", NK_TEXT_LEFT))
            SDL_OpenURL(PROJECT_URL);
        if (nk_menu_item_label(ctx, "Acerca de SpaceDesk Linux...", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_ABOUT);
        nk_menu_end(ctx);
    }
    nk_menubar_end(ctx);
    nk_end(ctx);
}

/* ---------- connect screen ---------- */

static void connect_screen(struct sdv_app *a, struct nk_rect area)
{
    struct nk_context *ctx = a->nk;
    struct sdv_server_info found[SDV_DISCOVERY_MAX];
    int n = a->disc ? sdv_discovery_list(a->disc, found, SDV_DISCOVERY_MAX) : 0;
    struct nk_rect r = nk_rect(area.x + (area.w - 520) / 2, area.y + 30, 520, 440);
    char line[256];

    if (area.w < 540)
        r.x = area.x + 10, r.w = area.w - 20;
    if (!nk_begin(ctx, "Conectar a un servidor spacedesk", r, NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
        nk_end(ctx);
        return;
    }
    nk_layout_row_dynamic(ctx, ROW, 1);
    nk_label(ctx, "Servidores encontrados en la red:", NK_TEXT_LEFT);
    if (!n)
        nk_label_colored(ctx, "   Buscando servidores...", NK_TEXT_LEFT, nk_rgb(150, 150, 150));
    for (int i = 0; i < n; i++) {
        snprintf(line, sizeof(line), "%s   (%s)", found[i].name, found[i].address);
        if (nk_button_label(ctx, line))
            app_connect(a, found[i].address);
    }

    nk_layout_row_dynamic(ctx, 8, 1);
    nk_spacing(ctx, 1);
    nk_layout_row_dynamic(ctx, ROW, 1);
    nk_label(ctx, "Dirección IP o nombre del equipo:", NK_TEXT_LEFT);
    nk_layout_row_template_begin(ctx, 28);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 110);
    nk_layout_row_template_end(ctx);
    {
        nk_flags ev = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, a->address,
                                                     sizeof(a->address), nk_filter_ascii);
        if ((nk_button_label(ctx, "Conectar") || (ev & NK_EDIT_COMMITED)) && a->address[0])
            app_connect(a, a->address);
    }

    if (a->set.recent[0][0]) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        nk_label(ctx, "Recientes:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, ROW, 3);
        for (int i = 0; i < SDV_MAX_RECENT && i < 6 && a->set.recent[i][0]; i++)
            if (nk_button_label(ctx, a->set.recent[i]))
                app_connect(a, a->set.recent[i]);
    }

    nk_layout_row_dynamic(ctx, 10, 1);
    nk_spacing(ctx, 1);
    if (a->message[0]) {
        nk_layout_row_dynamic(ctx, 40, 1);
        nk_label_colored_wrap(ctx, a->message, nk_rgb(240, 190, 90));
    }
    nk_layout_row_dynamic(ctx, 40, 1);
    nk_label_colored_wrap(ctx, "El PC con Windows debe tener instalado y activo el spacedesk Driver (servidor).",
                          nk_rgb(150, 150, 150));
    nk_end(ctx);
}

/* ---------- dialogs ---------- */

static int dialog_buttons(struct nk_context *ctx, const char *ok)
{
    int r = 0;
    nk_layout_row_dynamic(ctx, 8, 1);
    nk_spacing(ctx, 1);
    nk_layout_row_dynamic(ctx, 30, 2);
    if (nk_button_label(ctx, ok))
        r = 1;
    if (nk_button_label(ctx, "Cancelar"))
        r = -1;
    return r;
}

static void dialog_resolutions(struct sdv_app *a)
{
    static const int presets[][2] = {
        { 1024, 768 }, { 1280, 720 }, { 1280, 800 }, { 1366, 768 }, { 1600, 900 },
        { 1920, 1080 }, { 1920, 1200 }, { 2560, 1440 }, { 3840, 2160 },
    };
    static const char *names[] = {
        "1024 x 768", "1280 x 720", "1280 x 800", "1366 x 768", "1600 x 900",
        "1920 x 1080", "1920 x 1200", "2560 x 1440", "3840 x 2160",
    };
    struct nk_context *ctx = a->nk;
    struct sdv_settings *e = &a->edit;
    char label[96];
    int nw, nh, r;

    if (!nk_begin(ctx, "Resoluciones", centered(a, 360, 250), NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
        nk_end(ctx);
        return;
    }
    native_resolution(a, &nw, &nh);
    nk_layout_row_dynamic(ctx, ROW, 1);
    snprintf(label, sizeof(label), "Resolución nativa de pantalla (%d x %d)", nw, nh);
    option(ctx, label, &e->custom_resolution, 0);
    option(ctx, "Otra resolución", &e->custom_resolution, 1);
    if (e->custom_resolution) {
        int sel = -1;
        for (int i = 0; i < (int)(sizeof(presets) / sizeof(presets[0])); i++)
            if (presets[i][0] == e->custom_width && presets[i][1] == e->custom_height)
                sel = i;
        nk_layout_row_dynamic(ctx, 26, 1);
        r = nk_combo(ctx, names, (int)(sizeof(names) / sizeof(names[0])), sel < 0 ? 0 : sel, 22, nk_vec2(200, 220));
        if (r != sel && (sel >= 0 || r != 0)) {
            e->custom_width = presets[r][0];
            e->custom_height = presets[r][1];
        }
        nk_layout_row_dynamic(ctx, 26, 2);
        nk_property_int(ctx, "#Ancho:", 320, &e->custom_width, 7680, 2, 4);
        nk_property_int(ctx, "#Alto:", 240, &e->custom_height, 4320, 2, 4);
    }
    r = dialog_buttons(ctx, "Aplicar");
    if (r > 0) {
        a->set = *e;
        app_apply_settings(a, 1);
    }
    if (r)
        a->dialog = SDV_DLG_NONE;
    nk_end(ctx);
}

static void dialog_quality(struct sdv_app *a)
{
    struct nk_context *ctx = a->nk;
    struct sdv_settings *e = &a->edit;
    int compression = e->encoding != SDV_ENC_PREF_OFF;
    int r;

    if (!nk_begin(ctx, "Calidad / Rendimiento", centered(a, 430, 560), NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
        nk_end(ctx);
        return;
    }
    nk_layout_row_dynamic(ctx, ROW, 1);
    nk_label(ctx, "Compresión", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, ROW, 2);
    if (option(ctx, "Activada", &compression, 1))
        e->encoding = SDV_ENC_PREF_AUTO;
    if (option(ctx, "Desactivada (RGBA 32 bits)", &compression, 0))
        e->encoding = SDV_ENC_PREF_OFF;
    if (!compression) {
        nk_layout_row_dynamic(ctx, 36, 1);
        nk_label_colored_wrap(ctx, "¡Atención! Desactivar la compresión puede dar un rendimiento bajo.", nk_rgb(240, 190, 90));
    }

    separator(ctx);
    nk_label(ctx, "Codificación", NK_TEXT_LEFT);
    if (compression) {
        option(ctx, "JPEG por software (forzar siempre)", &e->encoding, SDV_ENC_PREF_JPEG);
        option(ctx, "Elegir la mejor (hardware o software)", &e->encoding, SDV_ENC_PREF_AUTO);
        option(ctx, "H.264 por hardware (forzar siempre)", &e->encoding, SDV_ENC_PREF_H264);
        nk_layout_row_dynamic(ctx, 50, 1);
        nk_label_colored_wrap(ctx, "H.264 por hardware requiere spacedesk Driver v2.2.02 o superior en Windows.",
                              nk_rgb(150, 150, 150));
    } else {
        nk_label_colored(ctx, "   (sin compresión)", NK_TEXT_LEFT, nk_rgb(110, 110, 110));
    }

    separator(ctx);
    nk_label(ctx, "Calidad vs. velocidad", NK_TEXT_LEFT);
    nk_layout_row_template_begin(ctx, ROW);
    nk_layout_row_template_push_static(ctx, 90);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 90);
    nk_layout_row_template_end(ctx);
    nk_label(ctx, "Máx. velocidad", NK_TEXT_LEFT);
    nk_slider_int(ctx, 1, &e->quality, 100, 1);
    nk_label(ctx, "Máx. calidad", NK_TEXT_RIGHT);
    nk_layout_row_dynamic(ctx, ROW, 1);
    check(ctx, "Turbo extra (reduce la resolución de la imagen)", &e->turbo);

    separator(ctx);
    nk_label(ctx, "Cuadros por segundo (FPS)", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, ROW, 4);
    option(ctx, "30", &e->fps, 30);
    option(ctx, "60", &e->fps, 60);
    option(ctx, "90", &e->fps, 90);
    option(ctx, "120", &e->fps, 120);

    r = dialog_buttons(ctx, "Aceptar");
    if (r > 0) {
        a->set = *e;
        app_apply_settings(a, 1);
    }
    if (r)
        a->dialog = SDV_DLG_NONE;
    nk_end(ctx);
}

static void dialog_autoconnect(struct sdv_app *a)
{
    struct nk_context *ctx = a->nk;
    struct sdv_settings *e = &a->edit;
    int r;

    if (!nk_begin(ctx, "Conexión automática", centered(a, 380, 220), NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
        nk_end(ctx);
        return;
    }
    nk_layout_row_dynamic(ctx, ROW, 1);
    nk_label(ctx, "Al iniciar, conectar:", NK_TEXT_LEFT);
    option(ctx, "Desactivada", &e->autoconnect, SDV_AUTOCONNECT_OFF);
    option(ctx, "A esta dirección (IP o nombre del equipo):", &e->autoconnect, SDV_AUTOCONNECT_ADDRESS);
    nk_layout_row_dynamic(ctx, 28, 1);
    nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, a->autoconnect_host, sizeof(a->autoconnect_host),
                                   nk_filter_ascii);
    r = dialog_buttons(ctx, "Aceptar");
    if (r > 0) {
        snprintf(e->server, sizeof(e->server), "%s", a->autoconnect_host);
        if (!e->server[0])
            e->autoconnect = SDV_AUTOCONNECT_OFF;
        a->set = *e;
        app_apply_settings(a, 0);
    }
    if (r)
        a->dialog = SDV_DLG_NONE;
    nk_end(ctx);
}

static void dialog_advanced(struct sdv_app *a)
{
    struct nk_context *ctx = a->nk;
    struct sdv_settings *e = &a->edit;
    int r;

    if (!nk_begin(ctx, "Avanzado", centered(a, 420, 200), NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
        nk_end(ctx);
        return;
    }
    nk_layout_row_dynamic(ctx, ROW, 1);
    nk_label(ctx, "Compresión", NK_TEXT_LEFT);
    check(ctx, "YUV sin comprimir (usa mucho ancho de banda)", &e->yuv);
    separator(ctx);
    nk_label(ctx, "Decodificación", NK_TEXT_LEFT);
    check(ctx, "H.264 por hardware en este equipo (VA-API)", &e->hw_decode);
    r = dialog_buttons(ctx, "Aceptar");
    if (r > 0) {
        a->set = *e;
        app_apply_settings(a, 1);
    }
    if (r)
        a->dialog = SDV_DLG_NONE;
    nk_end(ctx);
}

static void dialog_about(struct sdv_app *a)
{
    struct nk_context *ctx = a->nk;
    char line[160];

    if (!nk_begin(ctx, "Acerca de SpaceDesk Linux", centered(a, 440, 260), NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
        nk_end(ctx);
        return;
    }
    nk_layout_row_dynamic(ctx, ROW, 1);
    snprintf(line, sizeof(line), "SpaceDesk Linux v%s", SDV_VERSION);
    nk_label(ctx, line, NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 54, 1);
    nk_label_wrap(ctx, "Visor nativo no oficial para servidores spacedesk. Compatible gracias a la "
                       "documentación del protocolo obtenida para interoperabilidad.");
    nk_layout_row_dynamic(ctx, 36, 1);
    nk_label_colored_wrap(ctx, "spacedesk es una marca de datronicsoft Inc. Este proyecto no está afiliado.",
                          nk_rgb(150, 150, 150));
    nk_layout_row_dynamic(ctx, ROW, 1);
    snprintf(line, sizeof(line), "ID de dispositivo: %s", a->device_id);
    nk_label(ctx, line, NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 30, 2);
    if (nk_button_label(ctx, "GitHub"))
        SDL_OpenURL(PROJECT_URL);
    if (nk_button_label(ctx, "Cerrar"))
        a->dialog = SDV_DLG_NONE;
    nk_end(ctx);
}

/* ---------- status bar and toast ---------- */

static void status_bar(struct sdv_app *a, int width, int height)
{
    struct nk_context *ctx = a->nk;
    struct sdv_session_stats st = { 0 };
    char rx[32], tx[32], text[512];

    if (a->sess)
        sdv_session_get_stats(a->sess, &st);
    format_bytes(rx, sizeof(rx), st.bytes_rx);
    format_bytes(tx, sizeof(tx), st.bytes_tx);
    snprintf(text, sizeof(text),
             "IP local: %s    IP remota: %s    Res: %dx%d    Enviado: %s    Recibido: %s    Fps: %.0f    Mbps: %.1f    Comp: %s%s%s",
             st.local_addr, st.remote_addr, st.remote_w, st.remote_h, tx, rx, a->fps, a->mbps,
             sd_encoding_name(st.encoding),
             st.encoding == SD_ENC_H264 && st.h264_backend ? " " : "",
             st.encoding == SD_ENC_H264 && st.h264_backend ? st.h264_backend : "");
    if (nk_begin(ctx, "statusbar", nk_rect(0, (float)(height - SDV_STATUS_H), (float)width, SDV_STATUS_H),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_dynamic(ctx, 16, 1);
        nk_label(ctx, text, NK_TEXT_LEFT);
    }
    nk_end(ctx);
}

static void toast(struct sdv_app *a, int width, int height)
{
    struct nk_context *ctx = a->nk;
    float w = 560;

    if (!a->toast[0] || SDL_TICKS_PASSED(SDL_GetTicks(), a->toast_until))
        return;
    if (w > width - 20)
        w = (float)width - 20;
    if (nk_begin(ctx, "toast", nk_rect((width - w) / 2, (float)height - 90, w, 46),
                 NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT)) {
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label_wrap(ctx, a->toast);
    }
    nk_end(ctx);
}

/* ---------- frame ---------- */

void ui_frame(struct sdv_app *a)
{
    int w, h, top, bottom;
    int show_menu = !a->fullscreen || a->menu_open;
    int show_status = a->set.status_bar && a->streaming;
    struct nk_rect area;

    SDL_GetWindowSize(a->win, &w, &h);
    top = a->fullscreen ? 0 : SDV_MENU_H;
    bottom = show_status ? SDV_STATUS_H : 0;
    area = nk_rect(0, (float)top, (float)w, (float)(h - top - bottom));

    /* letterbox the remote desktop into the available area */
    a->video = (SDL_Rect){ (int)area.x, (int)area.y, (int)area.w, (int)area.h };
    if (a->remote_w > 0 && a->remote_h > 0 && area.w > 0 && area.h > 0) {
        double sx = area.w / a->remote_w, sy = area.h / a->remote_h, sc = sx < sy ? sx : sy;
        int vw = (int)(a->remote_w * sc), vh = (int)(a->remote_h * sc);
        a->video = (SDL_Rect){ (int)area.x + ((int)area.w - vw) / 2, (int)area.y + ((int)area.h - vh) / 2, vw, vh };
    }

    if (!a->streaming)
        connect_screen(a, area);
    if (show_status)
        status_bar(a, w, h);
    switch (a->dialog) {
    case SDV_DLG_RESOLUTIONS: dialog_resolutions(a); break;
    case SDV_DLG_QUALITY: dialog_quality(a); break;
    case SDV_DLG_AUTOCONNECT: dialog_autoconnect(a); break;
    case SDV_DLG_ADVANCED: dialog_advanced(a); break;
    case SDV_DLG_ABOUT: dialog_about(a); break;
    default: break;
    }
    toast(a, w, h);
    if (show_menu)
        menu_bar(a, w);
}
