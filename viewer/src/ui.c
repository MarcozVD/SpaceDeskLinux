/* User interface: menu bar, connect screen, settings dialogs and status bar (Nuklear). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "nk.h"
#include "protocol.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PROJECT_URL "https://github.com/MarcozVD/SpaceDeskLinux"
#define MANUAL_URL "https://www.spacedesk.net/user-manual/"
#define ROW 26
#define MENU_W_FILE 84
#define MENU_W_VIEW 54
#define MENU_W_FUNC 104
#define MENU_W_SETTINGS 84
#define MENU_W_HELP 70

/* ---------- palette (navy + blue) ---------- */

#define C_BG nk_rgb(11, 21, 36)
#define C_BAR nk_rgb(9, 17, 30)
#define C_PANEL nk_rgb(18, 33, 54)
#define C_CARD nk_rgb(24, 43, 70)
#define C_CARD_HOVER nk_rgb(30, 54, 88)
#define C_FIELD nk_rgb(10, 20, 35)
#define C_BORDER nk_rgb(39, 66, 104)
#define C_ACCENT nk_rgb(59, 130, 246)
#define C_ACCENT_HOVER nk_rgb(96, 165, 250)
#define C_ACCENT_ACTIVE nk_rgb(37, 99, 235)
#define C_ACCENT_SOFT nk_rgba(59, 130, 246, 40)
#define C_TEXT nk_rgb(230, 238, 248)
#define C_MUTED nk_rgb(143, 166, 196)
#define C_DIM nk_rgb(96, 116, 142)
#define C_OK nk_rgb(52, 211, 153)
#define C_WARN nk_rgb(251, 191, 36)
#define C_ERR nk_rgb(248, 113, 113)
#define C_WHITE nk_rgb(255, 255, 255)

static struct nk_font *f_regular, *f_bold, *f_title, *f_small;
static struct nk_style_button btn_primary, btn_ghost, btn_card;

static const nk_rune glyph_ranges[] = {
    0x0020, 0x00FF, /* Latin-1: Spanish accents */
    0x2013, 0x2026, /* dashes, bullet, ellipsis */
    0x2190, 0x2193, /* arrows */
    0x25CF, 0x25CF, /* black circle */
    0,
};

static const char *regular_fonts[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    NULL,
};
static const char *bold_fonts[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
    NULL,
};

static struct nk_font *load_font(struct nk_font_atlas *atlas, const char *const *paths, float size)
{
    struct nk_font_config cfg = nk_font_config(size);
    cfg.range = glyph_ranges;
    cfg.oversample_h = 2;
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "rb");
        if (f) {
            fclose(f);
            return nk_font_atlas_add_from_file(atlas, paths[i], size, &cfg);
        }
    }
    return NULL;
}

static void apply_theme(struct nk_context *ctx)
{
    struct nk_color t[NK_COLOR_COUNT];
    struct nk_style *s = &ctx->style;

    t[NK_COLOR_TEXT] = C_TEXT;
    t[NK_COLOR_WINDOW] = C_PANEL;
    t[NK_COLOR_HEADER] = C_BAR;
    t[NK_COLOR_BORDER] = C_BORDER;
    t[NK_COLOR_BUTTON] = C_CARD;
    t[NK_COLOR_BUTTON_HOVER] = C_CARD_HOVER;
    t[NK_COLOR_BUTTON_ACTIVE] = C_ACCENT_ACTIVE;
    t[NK_COLOR_TOGGLE] = C_FIELD;
    t[NK_COLOR_TOGGLE_HOVER] = C_CARD_HOVER;
    t[NK_COLOR_TOGGLE_CURSOR] = C_ACCENT;
    t[NK_COLOR_SELECT] = C_CARD;
    t[NK_COLOR_SELECT_ACTIVE] = C_ACCENT;
    t[NK_COLOR_SLIDER] = C_FIELD;
    t[NK_COLOR_SLIDER_CURSOR] = C_ACCENT;
    t[NK_COLOR_SLIDER_CURSOR_HOVER] = C_ACCENT_HOVER;
    t[NK_COLOR_SLIDER_CURSOR_ACTIVE] = C_ACCENT_ACTIVE;
    t[NK_COLOR_PROPERTY] = C_FIELD;
    t[NK_COLOR_EDIT] = C_FIELD;
    t[NK_COLOR_EDIT_CURSOR] = C_TEXT;
    t[NK_COLOR_COMBO] = C_FIELD;
    t[NK_COLOR_CHART] = C_FIELD;
    t[NK_COLOR_CHART_COLOR] = C_ACCENT;
    t[NK_COLOR_CHART_COLOR_HIGHLIGHT] = C_ERR;
    t[NK_COLOR_SCROLLBAR] = C_PANEL;
    t[NK_COLOR_SCROLLBAR_CURSOR] = C_BORDER;
    t[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = C_ACCENT_HOVER;
    t[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = C_ACCENT;
    t[NK_COLOR_TAB_HEADER] = C_BAR;
    t[NK_COLOR_KNOB] = C_FIELD;
    t[NK_COLOR_KNOB_CURSOR] = C_ACCENT;
    t[NK_COLOR_KNOB_CURSOR_HOVER] = C_ACCENT_HOVER;
    t[NK_COLOR_KNOB_CURSOR_ACTIVE] = C_ACCENT_ACTIVE;
    nk_style_from_table(ctx, t);

    s->window.rounding = 14;
    s->window.border = 1;
    s->window.padding = nk_vec2(18, 14);
    s->window.spacing = nk_vec2(8, 8);
    s->window.group_padding = nk_vec2(10, 8);
    s->window.header.padding = nk_vec2(16, 10);
    s->window.header.label_padding = nk_vec2(4, 2);
    s->window.popup_border = 1;
    s->window.menu_border = 1;
    s->window.combo_border = 1;
    s->window.popup_border_color = C_BORDER;
    s->window.menu_border_color = C_BORDER;
    s->window.combo_border_color = C_BORDER;
    s->window.popup_padding = nk_vec2(8, 8);
    s->window.menu_padding = nk_vec2(8, 8);
    s->window.combo_padding = nk_vec2(6, 6);

    s->button.rounding = 9;
    s->button.border = 1;
    s->button.border_color = C_BORDER;
    s->button.padding = nk_vec2(12, 4);

    s->menu_button.rounding = 7;
    s->menu_button.padding = nk_vec2(6, 2);
    s->menu_button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    s->menu_button.border = 0;
    s->menu_button.hover = nk_style_item_color(C_CARD_HOVER);
    s->menu_button.active = nk_style_item_color(C_ACCENT_ACTIVE);
    s->contextual_button.rounding = 7;
    s->contextual_button.hover = nk_style_item_color(C_ACCENT);
    s->contextual_button.text_hover = C_WHITE;

    s->edit.rounding = 9;
    s->edit.border = 1;
    s->edit.border_color = C_BORDER;
    s->edit.padding = nk_vec2(10, 6);
    s->edit.selected_normal = C_ACCENT;
    s->edit.selected_hover = C_ACCENT;
    s->property.rounding = 9;
    s->property.border = 1;
    s->property.border_color = C_BORDER;
    s->combo.rounding = 9;
    s->combo.border = 1;
    s->combo.border_color = C_BORDER;
    s->slider.rounding = 4;
    s->slider.bar_height = 6;
    s->slider.cursor_size = nk_vec2(18, 18);
    s->slider.bar_filled = C_ACCENT;
    s->checkbox.border = 1;
    s->checkbox.border_color = C_BORDER;
    s->option.border = 1;
    s->option.border_color = C_BORDER;

    btn_primary = s->button;
    btn_primary.normal = nk_style_item_color(C_ACCENT);
    btn_primary.hover = nk_style_item_color(C_ACCENT_HOVER);
    btn_primary.active = nk_style_item_color(C_ACCENT_ACTIVE);
    btn_primary.border_color = C_ACCENT;
    btn_primary.text_normal = C_WHITE;
    btn_primary.text_hover = C_WHITE;
    btn_primary.text_active = C_WHITE;

    btn_ghost = s->button;
    btn_ghost.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    btn_ghost.hover = nk_style_item_color(C_CARD_HOVER);
    btn_ghost.active = nk_style_item_color(C_CARD);
    btn_ghost.text_normal = C_MUTED;
    btn_ghost.text_hover = C_TEXT;

    btn_card = s->button;
    btn_card.normal = nk_style_item_color(C_CARD);
    btn_card.hover = nk_style_item_color(C_CARD_HOVER);
    btn_card.border_color = C_BORDER;
    btn_card.text_alignment = NK_TEXT_LEFT;
    btn_card.padding = nk_vec2(14, 4);
}

int ui_init(struct sdv_app *a)
{
    struct nk_font_atlas *atlas;

    a->nk = nk_sdl_init(a->win, a->ren);
    if (!a->nk)
        return -1;
    nk_sdl_font_stash_begin(&atlas);
    f_regular = load_font(atlas, regular_fonts, 16);
    f_small = load_font(atlas, regular_fonts, 13);
    f_bold = load_font(atlas, bold_fonts, 16);
    f_title = load_font(atlas, bold_fonts, 26);
    if (!f_regular)
        f_regular = nk_font_atlas_add_default(atlas, 15, NULL);
    if (!f_small)
        f_small = f_regular;
    if (!f_bold)
        f_bold = f_regular;
    if (!f_title)
        f_title = f_bold;
    nk_sdl_font_stash_end();
    nk_style_set_font(a->nk, &f_regular->handle);
    apply_theme(a->nk);
    return 0;
}

void ui_shutdown(void)
{
    nk_sdl_shutdown();
}

static int is_connecting(const struct sdv_app *a)
{
    return !a->streaming && a->host[0] && (a->sess || a->audio || a->reconnect_at);
}

int ui_wants_input(struct sdv_app *a)
{
    return !a->streaming || a->dialog != SDV_DLG_NONE || (a->fullscreen && a->menu_open) ||
           nk_window_is_any_hovered(a->nk) || nk_item_is_any_active(a->nk);
}

/* ---------- drawing helpers ---------- */

static void text_in(struct nk_context *ctx, struct nk_rect r, const char *s, struct nk_font *font,
                    struct nk_color fg, nk_flags align)
{
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    const struct nk_user_font *uf = &font->handle;
    int len = (int)strlen(s);
    float w = uf->width(uf->userdata, uf->height, s, len);
    float x = r.x;

    if (align & NK_TEXT_ALIGN_CENTERED)
        x = r.x + (r.w - w) / 2;
    else if (align & NK_TEXT_ALIGN_RIGHT)
        x = r.x + r.w - w;
    nk_draw_text(canvas, nk_rect(x, r.y + (r.h - uf->height) / 2, w + 2, uf->height), s, len, uf,
                 nk_rgba(0, 0, 0, 0), fg);
}

static void label_font(struct nk_context *ctx, const char *s, struct nk_font *font, struct nk_color c, nk_flags align)
{
    nk_style_push_font(ctx, &font->handle);
    nk_label_colored(ctx, s, align, c);
    nk_style_pop_font(ctx);
}

/* Section title; the caller sets the layout of the rows that follow. */
static void section(struct nk_context *ctx, const char *title)
{
    nk_layout_row_dynamic(ctx, 4, 1);
    nk_spacing(ctx, 1);
    nk_layout_row_dynamic(ctx, 20, 1);
    label_font(ctx, title, f_bold, C_ACCENT_HOVER, NK_TEXT_LEFT);
}

/* Section title followed by one-widget rows (options, checkboxes). */
static void section_list(struct nk_context *ctx, const char *title)
{
    section(ctx, title);
    nk_layout_row_dynamic(ctx, ROW, 1);
}

static void muted_wrap(struct nk_context *ctx, const char *s, float height)
{
    nk_layout_row_dynamic(ctx, height, 1);
    nk_style_push_font(ctx, &f_small->handle);
    nk_label_colored_wrap(ctx, s, C_MUTED);
    nk_style_pop_font(ctx);
}

/* Rounded colored strip with an accent bar on the left. */
static void banner(struct nk_context *ctx, const char *s, struct nk_color color)
{
    struct nk_rect r;
    nk_layout_row_dynamic(ctx, 40, 1);
    if (nk_widget(&r, ctx) == NK_WIDGET_INVALID)
        return;
    {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        nk_fill_rect(canvas, r, 9, nk_rgba(color.r, color.g, color.b, 34));
        nk_fill_rect(canvas, nk_rect(r.x, r.y, 4, r.h), 2, color);
        text_in(ctx, nk_rect(r.x + 14, r.y, r.w - 20, r.h), s, f_regular, C_TEXT, NK_TEXT_LEFT);
    }
}

static void spinner(struct nk_context *ctx, struct nk_rect r, struct nk_color color)
{
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    float t = (float)(SDL_GetTicks() % 1200) / 1200.0f * 2.0f * (float)M_PI;
    float cx = r.x + r.w / 2, cy = r.y + r.h / 2, rad = (r.w < r.h ? r.w : r.h) / 2 - 3;
    nk_stroke_circle(canvas, nk_rect(cx - rad, cy - rad, rad * 2, rad * 2), 3, nk_rgba(color.r, color.g, color.b, 50));
    nk_stroke_arc(canvas, cx, cy, rad, t, t + 1.9f, 3, color);
}

static const char *dots(void)
{
    static const char *d[] = { "", ".", "..", "..." };
    return d[(SDL_GetTicks() / 400) % 4];
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

static void separator(struct nk_context *ctx)
{
    nk_layout_row_dynamic(ctx, 7, 1);
    nk_rule_horizontal(ctx, C_BORDER, nk_false);
    nk_layout_row_dynamic(ctx, ROW, 1);
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

/* Right-aligned OK / Cancel buttons; returns 1 for OK, -1 for cancel. */
static int dialog_buttons(struct nk_context *ctx, const char *ok)
{
    int r = 0;
    nk_layout_row_dynamic(ctx, 10, 1);
    nk_spacing(ctx, 1);
    nk_layout_row_template_begin(ctx, 34);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 116);
    nk_layout_row_template_push_static(ctx, 116);
    nk_layout_row_template_end(ctx);
    nk_spacing(ctx, 1);
    if (nk_button_label_styled(ctx, &btn_ghost, "Cancelar"))
        r = -1;
    if (nk_button_label_styled(ctx, &btn_primary, ok))
        r = 1;
    return r;
}

static int begin_dialog(struct sdv_app *a, const char *title, float w, float h)
{
    /* keep the dialog above the backdrop that was focused just before */
    nk_window_set_focus(a->nk, title);
    return nk_begin(a->nk, title, centered(a, w, h),
                    NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE);
}

/* ---------- menu bar ---------- */

static void status_chip(struct sdv_app *a, struct nk_rect r)
{
    struct nk_context *ctx = a->nk;
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    char text[192];
    struct nk_color dot;

    if (a->streaming) {
        struct sdv_session_stats st = { 0 };
        if (a->sess)
            sdv_session_get_stats(a->sess, &st);
        dot = C_OK;
        if (a->sess)
            snprintf(text, sizeof(text), "Conectado a %s  ·  %s  ·  %.0f fps", a->host, sd_encoding_name(st.encoding), a->fps);
        else
            snprintf(text, sizeof(text), "Conectado a %s  ·  solo audio", a->host);
    } else if (is_connecting(a)) {
        dot = C_WARN;
        snprintf(text, sizeof(text), "Conectando a %s%s", a->host, dots());
    } else {
        dot = C_DIM;
        snprintf(text, sizeof(text), "Sin conexión");
    }
    {
        const struct nk_user_font *uf = &f_small->handle;
        float w = uf->width(uf->userdata, uf->height, text, (int)strlen(text));
        float x = r.x + r.w - w - 12;
        nk_fill_circle(canvas, nk_rect(x - 14, r.y + r.h / 2 - 4, 8, 8), dot);
        text_in(ctx, nk_rect(x, r.y, w + 4, r.h), text, f_small, C_MUTED, NK_TEXT_LEFT);
    }
}

static void menu_bar(struct sdv_app *a, int width)
{
    struct nk_context *ctx = a->nk;
    struct sdv_settings *s = &a->set;
    struct nk_rect chip;

    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_color(C_BAR));
    nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(8, 3));
    nk_style_push_float(ctx, &ctx->style.window.rounding, 0);
    nk_style_push_float(ctx, &ctx->style.window.border, 0);
    if (!nk_begin(ctx, "menubar", nk_rect(0, 0, (float)width, SDV_MENU_H), NK_WINDOW_NO_SCROLLBAR)) {
        nk_end(ctx);
        nk_style_pop_float(ctx);
        nk_style_pop_float(ctx);
        nk_style_pop_vec2(ctx);
        nk_style_pop_style_item(ctx);
        return;
    }
    nk_menubar_begin(ctx);
    nk_layout_row_begin(ctx, NK_STATIC, 20, 6);

    nk_layout_row_push(ctx, MENU_W_FILE);
    if (nk_menu_begin_label(ctx, "Archivo", NK_TEXT_CENTERED, nk_vec2(230, 120))) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (a->streaming || is_connecting(a)) {
            if (nk_menu_item_label(ctx, "Desconectar", NK_TEXT_LEFT))
                app_disconnect(a, "Desconectado");
        } else {
            nk_label_colored(ctx, "  Desconectar", NK_TEXT_LEFT, C_DIM);
        }
        if (nk_menu_item_label(ctx, "Salir", NK_TEXT_LEFT))
            a->quit = 1;
        nk_menu_end(ctx);
    }

    nk_layout_row_push(ctx, MENU_W_VIEW);
    if (nk_menu_begin_label(ctx, "Ver", NK_TEXT_CENTERED, nk_vec2(300, 110))) {
        int fs = a->fullscreen;
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (check(ctx, "Barra de estado", &s->status_bar))
            app_apply_settings(a, 0);
        if (check(ctx, "Pantalla completa        Alt+Enter", &fs))
            app_set_fullscreen(a, fs);
        nk_menu_end(ctx);
    }

    nk_layout_row_push(ctx, MENU_W_FUNC);
    if (nk_menu_begin_label(ctx, "Funciones", NK_TEXT_CENTERED, nk_vec2(320, 360))) {
        int reconnect = 0, changed = 0;
        nk_layout_row_dynamic(ctx, ROW, 1);
        reconnect |= check(ctx, "Pantalla", &s->display_enabled);
        changed |= check(ctx, "Teclado", &s->keyboard_enabled);
        separator(ctx);
        if (option(ctx, "Mouse remoto", &s->mouse_mode, SDV_MOUSE_REMOTE)) {
            changed = 1;
            app_toast(a, "Mouse remoto: haz clic en la pantalla para controlarlo");
        }
        if (option(ctx, "Mouse local (usar como táctil)", &s->mouse_mode, SDV_MOUSE_LOCAL_TOUCH)) {
            changed = 1;
            app_release_mouse(a);
            app_toast(a, "El mouse actúa como un dedo  ·  Alt+Shift para salir");
        }
        separator(ctx);
        changed |= option(ctx, "Pantalla táctil (absoluto)", &s->touch_mode, SDV_TOUCH_ABSOLUTE);
        changed |= option(ctx, "Touchpad / trackpad (relativo)", &s->touch_mode, SDV_TOUCH_RELATIVE);
        changed |= option(ctx, "Táctil apagado", &s->touch_mode, SDV_TOUCH_OFF);
        separator(ctx);
        reconnect |= check(ctx, "Audio (altavoz)", &s->audio_enabled);
        if (reconnect || changed)
            app_apply_settings(a, reconnect);
        nk_menu_end(ctx);
    }

    nk_layout_row_push(ctx, MENU_W_SETTINGS);
    if (nk_menu_begin_label(ctx, "Ajustes", NK_TEXT_CENTERED, nk_vec2(310, 330))) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (nk_menu_item_label(ctx, "Resoluciones…", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_RESOLUTIONS);
        if (nk_menu_item_label(ctx, "Calidad y rendimiento…", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_QUALITY);
        separator(ctx);
        if (nk_menu_item_label(ctx, "Conexión automática…", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_AUTOCONNECT);
        if (check(ctx, "Pantalla completa al conectar", &s->fullscreen_on_connect))
            app_apply_settings(a, 0);
        if (check(ctx, "Mantener monitor activo", &s->keep_monitor_active))
            app_apply_settings(a, 0);
        separator(ctx);
        if (nk_menu_item_label(ctx, "Avanzado…", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_ADVANCED);
        if (nk_menu_item_label(ctx, "Restablecer ajustes", NK_TEXT_LEFT)) {
            char recent[SDV_MAX_RECENT][SDV_HOST_LEN];
            memcpy(recent, s->recent, sizeof(recent));
            sdv_settings_defaults(s);
            memcpy(s->recent, recent, sizeof(recent));
            app_apply_settings(a, 1);
            app_toast(a, "Ajustes restablecidos");
        }
        nk_menu_end(ctx);
    }

    nk_layout_row_push(ctx, MENU_W_HELP);
    if (nk_menu_begin_label(ctx, "Ayuda", NK_TEXT_CENTERED, nk_vec2(310, 120))) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (nk_menu_item_label(ctx, "Manual de usuario de spacedesk", NK_TEXT_LEFT))
            SDL_OpenURL(MANUAL_URL);
        if (nk_menu_item_label(ctx, "Proyecto en GitHub", NK_TEXT_LEFT))
            SDL_OpenURL(PROJECT_URL);
        if (nk_menu_item_label(ctx, "Acerca de SpaceDesk Linux…", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_ABOUT);
        nk_menu_end(ctx);
    }

    nk_layout_row_push(ctx, (float)width - MENU_W_FILE - MENU_W_VIEW - MENU_W_FUNC - MENU_W_SETTINGS - MENU_W_HELP - 70);
    if (nk_widget(&chip, ctx) != NK_WIDGET_INVALID)
        status_chip(a, chip);
    nk_layout_row_end(ctx);
    nk_menubar_end(ctx);
    nk_end(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_vec2(ctx);
    nk_style_pop_style_item(ctx);
}

/* ---------- connect screen ---------- */

static void server_card(struct sdv_app *a, const char *name, const char *address)
{
    struct nk_context *ctx = a->nk;
    struct nk_rect r;

    nk_layout_row_template_begin(ctx, 52);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 118);
    nk_layout_row_template_end(ctx);
    if (nk_widget(&r, ctx) != NK_WIDGET_INVALID) {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        struct nk_rect full = nk_rect(r.x, r.y, r.w + 126, r.h);
        nk_fill_rect(canvas, full, 11, C_CARD);
        nk_stroke_rect(canvas, full, 11, 1, C_BORDER);
        nk_fill_circle(canvas, nk_rect(r.x + 16, r.y + r.h / 2 - 5, 10, 10), C_OK);
        text_in(ctx, nk_rect(r.x + 38, r.y + 6, r.w - 40, 22), name, f_bold, C_TEXT, NK_TEXT_LEFT);
        text_in(ctx, nk_rect(r.x + 38, r.y + 26, r.w - 40, 20), address, f_small, C_MUTED, NK_TEXT_LEFT);
    }
    nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(8, 8));
    {
        struct nk_style_button b = btn_primary;
        b.rounding = 9;
        if (nk_button_label_styled(ctx, &b, "Conectar"))
            app_connect(a, address);
    }
    nk_style_pop_vec2(ctx);
}

static void connect_screen(struct sdv_app *a, struct nk_rect area)
{
    struct nk_context *ctx = a->nk;
    struct sdv_server_info found[SDV_DISCOVERY_MAX];
    int n = a->disc ? sdv_discovery_list(a->disc, found, SDV_DISCOVERY_MAX) : 0;
    int cards;

    /* SDV_DEMO: fixed sample server for documentation screenshots (no real names or addresses) */
    if (getenv("SDV_DEMO")) {
        n = 1;
        snprintf(found[0].name, sizeof(found[0].name), "PC-ESCRITORIO");
        snprintf(found[0].address, sizeof(found[0].address), "192.168.1.50");
    }
    cards = n < 1 ? 1 : n > 4 ? 4 : n;
    /* height follows the content: header, servers, manual entry, recents, message, footer */
    float want = 380 + 62.0f * cards + (a->set.recent[0][0] ? 72 : 0) + (a->message[0] ? 52 : 0);
    float w = area.w - 40 < 600 ? area.w - 40 : 600;
    float h = is_connecting(a) ? 250 + (a->message[0] && a->reconnect_at ? 52 : 0) : want;
    nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR;
    struct nk_rect r;
    char line[256];

    if (h > area.h - 30) {
        h = area.h - 30;
        flags &= ~(nk_flags)NK_WINDOW_NO_SCROLLBAR;
    }
    r = nk_rect(area.x + (area.w - w) / 2, area.y + (area.h - h) / 2.4f, w, h);
    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_color(C_PANEL));
    if (!nk_begin(ctx, "connect", r, flags)) {
        nk_end(ctx);
        nk_style_pop_style_item(ctx);
        return;
    }

    /* header */
    nk_layout_row_dynamic(ctx, 36, 1);
    label_font(ctx, "SpaceDesk Linux", f_title, C_TEXT, NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 20, 1);
    label_font(ctx, "Usa este equipo como pantalla extra de tu PC con Windows", f_regular, C_MUTED, NK_TEXT_LEFT);

    if (is_connecting(a)) {
        struct nk_rect sp;
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_spacing(ctx, 1);
        nk_layout_row_template_begin(ctx, 44);
        nk_layout_row_template_push_static(ctx, 44);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);
        if (nk_widget(&sp, ctx) != NK_WIDGET_INVALID)
            spinner(ctx, sp, C_ACCENT);
        if (a->reconnect_at) {
            int secs = (int)((Sint32)(a->reconnect_at - SDL_GetTicks()) / 1000) + 1;
            snprintf(line, sizeof(line), "Reintentando conexión con %s en %d s", a->host, secs < 0 ? 0 : secs);
        } else {
            snprintf(line, sizeof(line), "Conectando a %s%s", a->host, dots());
        }
        label_font(ctx, line, f_bold, C_TEXT, NK_TEXT_LEFT);
        if (a->message[0] && a->reconnect_at)
            banner(ctx, a->message, C_WARN);
        nk_layout_row_template_begin(ctx, 34);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_static(ctx, 130);
        nk_layout_row_template_end(ctx);
        nk_spacing(ctx, 1);
        if (nk_button_label_styled(ctx, &btn_ghost, "Cancelar"))
            app_disconnect(a, "");
        nk_end(ctx);
        nk_style_pop_style_item(ctx);
        return;
    }

    section(ctx, "Servidores en tu red");
    if (!n) {
        struct nk_rect sp;
        nk_layout_row_template_begin(ctx, 40);
        nk_layout_row_template_push_static(ctx, 40);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);
        if (nk_widget(&sp, ctx) != NK_WIDGET_INVALID)
            spinner(ctx, sp, C_ACCENT);
        snprintf(line, sizeof(line), "Buscando servidores spacedesk%s", dots());
        label_font(ctx, line, f_regular, C_MUTED, NK_TEXT_LEFT);
    }
    for (int i = 0; i < n && i < 4; i++)
        server_card(a, found[i].name, found[i].address);

    section(ctx, "Conectar manualmente");
    nk_layout_row_template_begin(ctx, 36);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 118);
    nk_layout_row_template_end(ctx);
    {
        nk_flags ev = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, a->address,
                                                     sizeof(a->address), nk_filter_ascii);
        if ((nk_button_label_styled(ctx, &btn_primary, "Conectar") || (ev & NK_EDIT_COMMITED)) && a->address[0])
            app_connect(a, a->address);
    }
    muted_wrap(ctx, "Dirección IP o nombre del PC con Windows, por ejemplo 192.168.1.20", 18);

    if (a->set.recent[0][0]) {
        section(ctx, "Recientes");
        nk_layout_row_dynamic(ctx, 30, 3);
        for (int i = 0; i < SDV_MAX_RECENT && i < 3 && a->set.recent[i][0]; i++)
            if (nk_button_label_styled(ctx, &btn_card, a->set.recent[i]))
                app_connect(a, a->set.recent[i]);
    }

    if (a->message[0])
        banner(ctx, a->message, C_WARN);

    nk_layout_row_dynamic(ctx, 8, 1);
    nk_spacing(ctx, 1);
    muted_wrap(ctx, "Requiere spacedesk Driver activo en Windows.   Alt+Enter: pantalla completa   ·   Ctrl+Alt+M: menú", 34);
    nk_end(ctx);
    nk_style_pop_style_item(ctx);
}

/* ---------- dialogs ---------- */

static void dialog_resolutions(struct sdv_app *a)
{
    static const int presets[][2] = {
        { 1280, 720 }, { 1280, 800 }, { 1366, 768 }, { 1600, 900 }, { 1920, 1080 },
        { 1920, 1200 }, { 2560, 1440 }, { 3840, 2160 }, { 1024, 768 },
    };
    static const char *names[] = {
        "1280 × 720 (HD)", "1280 × 800", "1366 × 768", "1600 × 900", "1920 × 1080 (Full HD)",
        "1920 × 1200", "2560 × 1440 (QHD)", "3840 × 2160 (4K)", "1024 × 768",
    };
    struct nk_context *ctx = a->nk;
    struct sdv_settings *e = &a->edit;
    char label[96];
    int nw, nh, r, count = (int)(sizeof(presets) / sizeof(presets[0]));

    if (!begin_dialog(a, "Resoluciones", 420, 340)) {
        nk_end(ctx);
        return;
    }
    native_resolution(a, &nw, &nh);
    section_list(ctx, "Resolución del monitor extra");
    snprintf(label, sizeof(label), "Nativa de esta pantalla (%d × %d)", nw, nh);
    option(ctx, label, &e->custom_resolution, 0);
    option(ctx, "Personalizada", &e->custom_resolution, 1);
    if (e->custom_resolution) {
        int sel = -1;
        for (int i = 0; i < count; i++)
            if (presets[i][0] == e->custom_width && presets[i][1] == e->custom_height)
                sel = i;
        nk_layout_row_dynamic(ctx, 30, 1);
        r = nk_combo(ctx, names, count, sel < 0 ? 0 : sel, 26, nk_vec2(260, 260));
        if (r != sel && (sel >= 0 || r != 0)) {
            e->custom_width = presets[r][0];
            e->custom_height = presets[r][1];
        }
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_property_int(ctx, "#Ancho:", 320, &e->custom_width, 7680, 2, 4);
        nk_property_int(ctx, "#Alto:", 240, &e->custom_height, 4320, 2, 4);
    }
    muted_wrap(ctx, "Windows verá un monitor nuevo con esta resolución. Los cambios reconectan la sesión.", 36);
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
    char q[32];
    int r;

    if (!begin_dialog(a, "Calidad y rendimiento", 520, 660)) {
        nk_end(ctx);
        return;
    }
    section(ctx, "Compresión");
    nk_layout_row_dynamic(ctx, ROW, 2);
    if (option(ctx, "Activada", &compression, 1))
        e->encoding = SDV_ENC_PREF_AUTO;
    if (option(ctx, "Desactivada (RGBA 32 bits)", &compression, 0))
        e->encoding = SDV_ENC_PREF_OFF;
    if (!compression)
        banner(ctx, "Sin compresión el rendimiento puede ser bajo", C_WARN);

    section(ctx, "Codificación");
    if (compression) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        option(ctx, "Elegir la mejor (hardware o software)", &e->encoding, SDV_ENC_PREF_AUTO);
        option(ctx, "H.264 por hardware (forzar siempre)", &e->encoding, SDV_ENC_PREF_H264);
        option(ctx, "JPEG por software (forzar siempre)", &e->encoding, SDV_ENC_PREF_JPEG);
        muted_wrap(ctx, "H.264 da la mejor fluidez; requiere spacedesk Driver 2.2.02 o superior.", 20);
    } else {
        muted_wrap(ctx, "No aplica sin compresión.", 20);
    }

    section(ctx, "Calidad vs. velocidad");
    nk_layout_row_template_begin(ctx, ROW);
    nk_layout_row_template_push_static(ctx, 70);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 70);
    nk_layout_row_template_end(ctx);
    nk_style_push_font(ctx, &f_small->handle);
    nk_label_colored(ctx, "Velocidad", NK_TEXT_LEFT, C_MUTED);
    nk_style_pop_font(ctx);
    nk_slider_int(ctx, 1, &e->quality, 100, 1);
    nk_style_push_font(ctx, &f_small->handle);
    nk_label_colored(ctx, "Calidad", NK_TEXT_RIGHT, C_MUTED);
    nk_style_pop_font(ctx);
    nk_layout_row_dynamic(ctx, 18, 1);
    snprintf(q, sizeof(q), "%d %%", e->quality);
    label_font(ctx, q, f_small, C_ACCENT_HOVER, NK_TEXT_CENTERED);
    nk_layout_row_dynamic(ctx, ROW, 1);
    check(ctx, "Turbo (reduce la resolución para ganar velocidad)", &e->turbo);

    section(ctx, "Cuadros por segundo");
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

    if (!begin_dialog(a, "Conexión automática", 440, 300)) {
        nk_end(ctx);
        return;
    }
    section_list(ctx, "Al abrir la aplicación");
    option(ctx, "Mostrar la pantalla de conexión", &e->autoconnect, SDV_AUTOCONNECT_OFF);
    option(ctx, "Conectar automáticamente a:", &e->autoconnect, SDV_AUTOCONNECT_ADDRESS);
    nk_layout_row_dynamic(ctx, 34, 1);
    nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, a->autoconnect_host, sizeof(a->autoconnect_host),
                                   nk_filter_ascii);
    muted_wrap(ctx, "IP o nombre del PC. Si la conexión se corta, se reintenta sola.", 20);
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

    if (!begin_dialog(a, "Avanzado", 460, 300)) {
        nk_end(ctx);
        return;
    }
    section_list(ctx, "Compresión");
    check(ctx, "YUV sin comprimir", &e->yuv);
    muted_wrap(ctx, "Imagen sin pérdidas; necesita una red muy rápida (cable).", 20);
    section_list(ctx, "Decodificación");
    check(ctx, "H.264 por hardware en este equipo (VA-API)", &e->hw_decode);
    muted_wrap(ctx, "Desactívalo solo si ves artefactos o la imagen no aparece.", 20);
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

    if (!begin_dialog(a, "Acerca de", 460, 330)) {
        nk_end(ctx);
        return;
    }
    nk_layout_row_dynamic(ctx, 34, 1);
    label_font(ctx, "SpaceDesk Linux", f_title, C_TEXT, NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 18, 1);
    snprintf(line, sizeof(line), "Versión %s", SDV_VERSION);
    label_font(ctx, line, f_small, C_ACCENT_HOVER, NK_TEXT_LEFT);
    muted_wrap(ctx, "Visor nativo y libre para usar un equipo Linux como monitor extra de un PC con "
                    "Windows que ejecuta spacedesk.", 40);
    muted_wrap(ctx, "spacedesk es una marca de datronicsoft Inc. Proyecto independiente, sin afiliación.", 34);
    nk_layout_row_dynamic(ctx, 18, 1);
    snprintf(line, sizeof(line), "ID de este dispositivo: %s", a->device_id);
    label_font(ctx, line, f_small, C_DIM, NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 10, 1);
    nk_spacing(ctx, 1);
    nk_layout_row_template_begin(ctx, 34);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 116);
    nk_layout_row_template_push_static(ctx, 116);
    nk_layout_row_template_end(ctx);
    nk_spacing(ctx, 1);
    if (nk_button_label_styled(ctx, &btn_ghost, "GitHub"))
        SDL_OpenURL(PROJECT_URL);
    if (nk_button_label_styled(ctx, &btn_primary, "Cerrar"))
        a->dialog = SDV_DLG_NONE;
    nk_end(ctx);
}

/* ---------- overlays ---------- */

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
             "Local %s   ·   Remoto %s   ·   %d × %d   ·   %.0f fps   ·   %.1f Mbps   ·   %s%s%s   ·   ↓ %s   ↑ %s",
             st.local_addr, st.remote_addr, st.remote_w, st.remote_h, a->fps, a->mbps, sd_encoding_name(st.encoding),
             st.encoding == SD_ENC_H264 && st.h264_backend ? " " : "",
             st.encoding == SD_ENC_H264 && st.h264_backend ? st.h264_backend : "", rx, tx);
    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_color(C_BAR));
    nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(12, 3));
    nk_style_push_float(ctx, &ctx->style.window.rounding, 0);
    nk_style_push_float(ctx, &ctx->style.window.border, 0);
    if (nk_begin(ctx, "statusbar", nk_rect(0, (float)(height - SDV_STATUS_H), (float)width, SDV_STATUS_H),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_dynamic(ctx, 16, 1);
        label_font(ctx, text, f_small, C_MUTED, NK_TEXT_LEFT);
    }
    nk_end(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_vec2(ctx);
    nk_style_pop_style_item(ctx);
}

/* Floating pill with text; used for toasts and hints. */
static void pill(struct nk_context *ctx, const char *name, const char *text, float cx, float y, struct nk_color bg)
{
    const struct nk_user_font *uf = &f_regular->handle;
    float w = uf->width(uf->userdata, uf->height, text, (int)strlen(text)) + 44;

    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_color(bg));
    nk_style_push_float(ctx, &ctx->style.window.rounding, 18);
    nk_style_push_float(ctx, &ctx->style.window.border, 0);
    nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(22, 6));
    if (nk_begin(ctx, name, nk_rect(cx - w / 2, y, w, 38), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT)) {
        nk_layout_row_dynamic(ctx, 26, 1);
        nk_label_colored(ctx, text, NK_TEXT_CENTERED, C_WHITE);
    }
    nk_end(ctx);
    nk_style_pop_vec2(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_style_item(ctx);
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

    /* hint until the user captures the mouse */
    if (a->streaming && a->sess && a->set.mouse_mode == SDV_MOUSE_REMOTE && !a->mouse_captured &&
        a->dialog == SDV_DLG_NONE && !a->menu_open)
        pill(a->nk, "hint", "Haz clic en la pantalla para controlar el PC   ·   Ctrl+Alt+M libera el mouse",
             (float)w / 2, area.y + 14, nk_rgba(9, 17, 30, 215));

    /* dim everything behind an open dialog */
    if (a->dialog != SDV_DLG_NONE) {
        nk_style_push_style_item(a->nk, &a->nk->style.window.fixed_background, nk_style_item_color(nk_rgba(4, 9, 18, 150)));
        nk_style_push_float(a->nk, &a->nk->style.window.border, 0);
        nk_style_push_float(a->nk, &a->nk->style.window.rounding, 0);
        if (nk_begin(a->nk, "backdrop", nk_rect(0, 0, (float)w, (float)h), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT))
            nk_window_set_focus(a->nk, "backdrop");
        nk_end(a->nk);
        nk_style_pop_float(a->nk);
        nk_style_pop_float(a->nk);
        nk_style_pop_style_item(a->nk);
    }

    switch (a->dialog) {
    case SDV_DLG_RESOLUTIONS: dialog_resolutions(a); break;
    case SDV_DLG_QUALITY: dialog_quality(a); break;
    case SDV_DLG_AUTOCONNECT: dialog_autoconnect(a); break;
    case SDV_DLG_ADVANCED: dialog_advanced(a); break;
    case SDV_DLG_ABOUT: dialog_about(a); break;
    default: break;
    }
    if (a->toast[0] && !SDL_TICKS_PASSED(SDL_GetTicks(), a->toast_until))
        pill(a->nk, "toast", a->toast, (float)w / 2, (float)h - bottom - 64, C_ACCENT_ACTIVE);
    if (show_menu)
        menu_bar(a, w);
}
