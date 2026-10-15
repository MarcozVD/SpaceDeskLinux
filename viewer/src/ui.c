/* User interface: menu bar, connect screen, settings dialogs and status bar (Nuklear). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "nk.h"
#include "protocol.h"

#define PROJECT_URL "https://github.com/MarcozVD/SpaceDeskLinux"
#define MANUAL_URL "https://www.spacedesk.net/user-manual/"
#define ROW 26
#define MENU_W_FILE 76
#define MENU_W_VIEW 48
#define MENU_W_FUNC 92
#define MENU_W_SETTINGS 76
#define MENU_W_HELP 64

/* ---------- palette: graphite with one amber accent ---------- */

#define C_BG nk_rgb(23, 24, 26)
#define C_BAR nk_rgb(17, 18, 20)
#define C_SURFACE nk_rgb(31, 32, 35)
#define C_RAISED nk_rgb(41, 42, 46)
#define C_FIELD nk_rgb(19, 20, 22)
#define C_LINE nk_rgb(52, 54, 58)
#define C_TEXT nk_rgb(232, 230, 227)
#define C_MUTED nk_rgb(156, 153, 147)
#define C_DIM nk_rgb(106, 104, 99)
#define C_ACCENT nk_rgb(233, 162, 59)
#define C_ACCENT_HOVER nk_rgb(243, 184, 98)
#define C_ACCENT_ACTIVE nk_rgb(204, 138, 40)
#define C_ON_ACCENT nk_rgb(30, 22, 10)
#define C_OK nk_rgb(112, 191, 122)
#define C_ERR nk_rgb(228, 108, 96)
#define C_CLEAR nk_rgba(0, 0, 0, 0)

static struct nk_font *f_regular, *f_bold, *f_title, *f_small, *f_mono, *f_mono_small;
static struct nk_style_button btn_primary, btn_secondary, btn_quiet;

static const nk_rune glyph_ranges[] = {
    0x0020, 0x00FF, /* Latin-1: Spanish accents, middle dot, multiplication sign */
    0x2013, 0x2026, /* dashes, ellipsis */
    0,
};

/* Cantarell is the GNOME interface font; DejaVu is on every Debian desktop. */
static const char *regular_fonts[] = {
    "/usr/share/fonts/opentype/cantarell/Cantarell-Regular.otf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    NULL,
};
static const char *bold_fonts[] = {
    "/usr/share/fonts/opentype/cantarell/Cantarell-Bold.otf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
    NULL,
};
static const char *mono_fonts[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
    "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf",
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
    t[NK_COLOR_WINDOW] = C_SURFACE;
    t[NK_COLOR_HEADER] = C_SURFACE;
    t[NK_COLOR_BORDER] = C_LINE;
    t[NK_COLOR_BUTTON] = C_RAISED;
    t[NK_COLOR_BUTTON_HOVER] = nk_rgb(50, 51, 56);
    t[NK_COLOR_BUTTON_ACTIVE] = C_LINE;
    t[NK_COLOR_TOGGLE] = C_FIELD;
    t[NK_COLOR_TOGGLE_HOVER] = C_RAISED;
    t[NK_COLOR_TOGGLE_CURSOR] = C_ACCENT;
    t[NK_COLOR_SELECT] = C_RAISED;
    t[NK_COLOR_SELECT_ACTIVE] = C_ACCENT;
    t[NK_COLOR_SLIDER] = C_LINE;
    t[NK_COLOR_SLIDER_CURSOR] = C_ACCENT;
    t[NK_COLOR_SLIDER_CURSOR_HOVER] = C_ACCENT_HOVER;
    t[NK_COLOR_SLIDER_CURSOR_ACTIVE] = C_ACCENT_ACTIVE;
    t[NK_COLOR_PROPERTY] = C_FIELD;
    t[NK_COLOR_EDIT] = C_FIELD;
    t[NK_COLOR_EDIT_CURSOR] = C_ACCENT;
    t[NK_COLOR_COMBO] = C_FIELD;
    t[NK_COLOR_CHART] = C_FIELD;
    t[NK_COLOR_CHART_COLOR] = C_ACCENT;
    t[NK_COLOR_CHART_COLOR_HIGHLIGHT] = C_ERR;
    t[NK_COLOR_SCROLLBAR] = C_SURFACE;
    t[NK_COLOR_SCROLLBAR_CURSOR] = C_LINE;
    t[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = C_DIM;
    t[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = C_MUTED;
    t[NK_COLOR_TAB_HEADER] = C_BAR;
    t[NK_COLOR_KNOB] = C_FIELD;
    t[NK_COLOR_KNOB_CURSOR] = C_ACCENT;
    t[NK_COLOR_KNOB_CURSOR_HOVER] = C_ACCENT_HOVER;
    t[NK_COLOR_KNOB_CURSOR_ACTIVE] = C_ACCENT_ACTIVE;
    nk_style_from_table(ctx, t);

    s->window.rounding = 6;
    s->window.border = 1;
    s->window.padding = nk_vec2(20, 12);
    s->window.spacing = nk_vec2(8, 6);
    s->window.group_padding = nk_vec2(10, 8);
    s->window.scrollbar_size = nk_vec2(6, 6);
    s->scrollv.rounding = 3;
    s->scrollv.rounding_cursor = 3;
    s->scrollv.border = 0;
    s->scrollv.border_cursor = 0;
    s->window.header.padding = nk_vec2(20, 10);
    s->window.header.label_padding = nk_vec2(0, 2);
    s->window.header.label_normal = C_TEXT;
    s->window.header.label_hover = C_TEXT;
    s->window.header.label_active = C_TEXT;
    s->window.popup_border = 1;
    s->window.menu_border = 1;
    s->window.combo_border = 1;
    s->window.popup_border_color = C_LINE;
    s->window.menu_border_color = C_LINE;
    s->window.combo_border_color = C_LINE;
    s->window.popup_padding = nk_vec2(6, 6);
    s->window.menu_padding = nk_vec2(6, 6);
    s->window.combo_padding = nk_vec2(6, 6);

    s->button.rounding = 4;
    s->button.border = 1;
    s->button.border_color = C_LINE;
    s->button.padding = nk_vec2(12, 4);

    s->menu_button.rounding = 3;
    s->menu_button.padding = nk_vec2(6, 2);
    s->menu_button.normal = nk_style_item_color(C_CLEAR);
    s->menu_button.border = 0;
    s->menu_button.hover = nk_style_item_color(C_RAISED);
    s->menu_button.active = nk_style_item_color(C_RAISED);
    s->contextual_button.rounding = 3;
    s->contextual_button.hover = nk_style_item_color(C_RAISED);
    s->contextual_button.active = nk_style_item_color(C_LINE);
    s->contextual_button.text_hover = C_TEXT;

    s->edit.rounding = 4;
    s->edit.border = 1;
    s->edit.border_color = C_LINE;
    s->edit.padding = nk_vec2(10, 6);
    s->edit.selected_normal = C_ACCENT;
    s->edit.selected_hover = C_ACCENT;
    s->edit.selected_text_normal = C_ON_ACCENT;
    s->edit.selected_text_hover = C_ON_ACCENT;
    s->property.rounding = 4;
    s->property.border = 1;
    s->property.border_color = C_LINE;
    s->combo.rounding = 4;
    s->combo.border = 1;
    s->combo.border_color = C_LINE;
    s->combo.button.normal = nk_style_item_color(C_CLEAR);
    s->combo.button.hover = nk_style_item_color(C_CLEAR);
    s->combo.button.active = nk_style_item_color(C_CLEAR);
    s->combo.button.text_normal = C_MUTED;
    s->combo.button.text_hover = C_TEXT;
    s->combo.button.text_active = C_TEXT;
    s->combo.button_padding = nk_vec2(8, 8);
    s->combo.sym_normal = NK_SYMBOL_TRIANGLE_DOWN;
    s->slider.rounding = 2;
    s->slider.bar_height = 4;
    s->slider.cursor_size = nk_vec2(14, 14);
    s->slider.bar_normal = C_LINE;
    s->slider.bar_hover = C_LINE;
    s->slider.bar_active = C_LINE;
    s->slider.bar_filled = C_ACCENT;
    s->checkbox.border = 1;
    s->checkbox.border_color = C_LINE;
    s->checkbox.padding = nk_vec2(3, 3);
    s->option.border = 1;
    s->option.border_color = C_LINE;
    s->option.padding = nk_vec2(4, 4);

    btn_primary = s->button;
    btn_primary.normal = nk_style_item_color(C_ACCENT);
    btn_primary.hover = nk_style_item_color(C_ACCENT_HOVER);
    btn_primary.active = nk_style_item_color(C_ACCENT_ACTIVE);
    btn_primary.border_color = C_ACCENT;
    btn_primary.text_normal = C_ON_ACCENT;
    btn_primary.text_hover = C_ON_ACCENT;
    btn_primary.text_active = C_ON_ACCENT;

    btn_secondary = s->button;
    btn_secondary.text_normal = C_TEXT;
    btn_secondary.text_hover = C_TEXT;

    btn_quiet = s->button;
    btn_quiet.normal = nk_style_item_color(C_CLEAR);
    btn_quiet.hover = nk_style_item_color(C_RAISED);
    btn_quiet.active = nk_style_item_color(C_LINE);
    btn_quiet.border = 0;
    btn_quiet.text_normal = C_MUTED;
    btn_quiet.text_hover = C_TEXT;
    btn_quiet.text_alignment = NK_TEXT_LEFT;
}

int ui_init(struct sdv_app *a)
{
    struct nk_font_atlas *atlas;

    a->nk = nk_sdl_init(a->win, a->ren);
    if (!a->nk)
        return -1;
    nk_sdl_font_stash_begin(&atlas);
    f_regular = load_font(atlas, regular_fonts, 16);
    f_small = load_font(atlas, regular_fonts, 14);
    f_bold = load_font(atlas, bold_fonts, 16);
    f_title = load_font(atlas, bold_fonts, 28);
    f_mono = load_font(atlas, mono_fonts, 14);
    f_mono_small = load_font(atlas, mono_fonts, 12);
    if (!f_regular)
        f_regular = nk_font_atlas_add_default(atlas, 15, NULL);
    if (!f_small)
        f_small = f_regular;
    if (!f_bold)
        f_bold = f_regular;
    if (!f_title)
        f_title = f_bold;
    if (!f_mono)
        f_mono = f_regular;
    if (!f_mono_small)
        f_mono_small = f_small;
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

static float text_width(struct nk_font *font, const char *s)
{
    const struct nk_user_font *uf = &font->handle;
    return uf->width(uf->userdata, uf->height, s, (int)strlen(s));
}

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
    nk_draw_text(canvas, nk_rect(x, r.y + (r.h - uf->height) / 2, w + 2, uf->height), s, len, uf, C_CLEAR, fg);
}

static void label_font(struct nk_context *ctx, const char *s, struct nk_font *font, struct nk_color c, nk_flags align)
{
    nk_style_push_font(ctx, &font->handle);
    nk_label_colored(ctx, s, align, c);
    nk_style_pop_font(ctx);
}

static void gap(struct nk_context *ctx, float h)
{
    nk_layout_row_dynamic(ctx, h, 1);
    nk_spacing(ctx, 1);
}

/* Section title in a dialog; the caller sets the layout of the rows that follow. */
static void section(struct nk_context *ctx, const char *title)
{
    gap(ctx, 6);
    nk_layout_row_dynamic(ctx, 22, 1);
    label_font(ctx, title, f_bold, C_TEXT, NK_TEXT_LEFT);
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

/* Message line with a coloured mark on the left. */
static void notice(struct nk_context *ctx, const char *s, struct nk_color color)
{
    struct nk_rect r;
    nk_layout_row_dynamic(ctx, 36, 1);
    if (nk_widget(&r, ctx) == NK_WIDGET_INVALID)
        return;
    {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        nk_fill_rect(canvas, r, 4, C_RAISED);
        nk_fill_rect(canvas, nk_rect(r.x, r.y, 3, r.h), 0, color);
        text_in(ctx, nk_rect(r.x + 14, r.y, r.w - 20, r.h), s, f_small, C_TEXT, NK_TEXT_LEFT);
    }
}

/* Thin indeterminate progress line. */
static void progress_line(struct nk_context *ctx, struct nk_rect r)
{
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    float t = (float)(SDL_GetTicks() % 1600) / 1600.0f;
    float seg = r.w * 0.28f;
    float x = r.x - seg + (r.w + seg) * t;
    float x0 = x < r.x ? r.x : x, x1 = x + seg > r.x + r.w ? r.x + r.w : x + seg;

    nk_fill_rect(canvas, nk_rect(r.x, r.y, r.w, 2), 0, C_LINE);
    if (x1 > x0)
        nk_fill_rect(canvas, nk_rect(x0, r.y, x1 - x0, 2), 0, C_ACCENT);
}

static const char *dots(void)
{
    static const char *d[] = { "", ".", "..", "..." };
    return d[(SDL_GetTicks() / 400) % 4];
}

/* Draws a key cap; returns its width. */
static float keycap(struct nk_command_buffer *canvas, float x, float y, const char *key)
{
    const struct nk_user_font *uf = &f_mono_small->handle;
    float w = text_width(f_mono_small, key) + 12, h = 20;
    nk_fill_rect(canvas, nk_rect(x, y, w, h), 3, C_RAISED);
    nk_stroke_rect(canvas, nk_rect(x, y, w, h), 3, 1, C_LINE);
    nk_fill_rect(canvas, nk_rect(x + 1, y + h - 2, w - 2, 1), 0, C_FIELD);
    nk_draw_text(canvas, nk_rect(x + 6, y + (h - 2 - uf->height) / 2, w, uf->height), key, (int)strlen(key), uf,
                 C_CLEAR, C_TEXT);
    return w;
}

/* One shortcut row: keys joined by "+", then a description. */
static void shortcut(struct nk_context *ctx, const char *const *keys, const char *what)
{
    struct nk_rect r;
    nk_layout_row_dynamic(ctx, 26, 1);
    if (nk_widget(&r, ctx) == NK_WIDGET_INVALID)
        return;
    {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        float x = r.x;
        for (int i = 0; keys[i]; i++) {
            if (i) {
                text_in(ctx, nk_rect(x + 3, r.y, 10, r.h), "+", f_small, C_DIM, NK_TEXT_LEFT);
                x += 14;
            }
            x += keycap(canvas, x, r.y + 3, keys[i]);
        }
        text_in(ctx, nk_rect(x + 12, r.y, r.w - (x - r.x) - 12, r.h), what, f_small, C_MUTED, NK_TEXT_LEFT);
    }
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
    nk_rule_horizontal(ctx, C_LINE, nk_false);
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

/* Right-aligned Cancel / OK buttons; returns 1 for OK, -1 for cancel. */
static int dialog_buttons(struct nk_context *ctx, const char *ok)
{
    int r = 0;
    gap(ctx, 10);
    nk_layout_row_template_begin(ctx, 32);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 104);
    nk_layout_row_template_push_static(ctx, 104);
    nk_layout_row_template_end(ctx);
    nk_spacing(ctx, 1);
    if (nk_button_label_styled(ctx, &btn_secondary, "Cancelar"))
        r = -1;
    if (nk_button_label_styled(ctx, &btn_primary, ok))
        r = 1;
    return r;
}

static int begin_dialog(struct sdv_app *a, const char *title, float w, float h)
{
    struct nk_context *ctx = a->nk;
    struct nk_rect r;
    int open;

    /* keep the dialog above the backdrop that was focused just before */
    nk_window_set_focus(ctx, title);
    nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(22, 14));
    open = nk_begin(ctx, title, centered(a, w, h), NK_WINDOW_BORDER);
    nk_style_pop_vec2(ctx);
    if (open) {
        nk_layout_row_dynamic(ctx, 30, 1);
        if (nk_widget(&r, ctx) != NK_WIDGET_INVALID) {
            text_in(ctx, nk_rect(r.x, r.y, r.w, r.h - 6), title, f_bold, C_TEXT, NK_TEXT_LEFT);
            nk_fill_rect(nk_window_get_canvas(ctx), nk_rect(r.x - 22, r.y + r.h - 1, r.w + 44, 1), 0, C_LINE);
        }
    }
    return open;
}

/* ---------- menu bar ---------- */

static void status_text(struct sdv_app *a, struct nk_rect r)
{
    struct nk_context *ctx = a->nk;
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    char text[192];
    struct nk_color mark;
    float w, x;

    if (a->streaming) {
        struct sdv_session_stats st = { 0 };
        if (a->sess)
            sdv_session_get_stats(a->sess, &st);
        mark = C_OK;
        if (a->sess)
            snprintf(text, sizeof(text), "%s   %s   %3.0f fps", a->host, sd_encoding_name(st.encoding), a->fps);
        else
            snprintf(text, sizeof(text), "%s   solo audio", a->host);
    } else if (is_connecting(a)) {
        mark = C_ACCENT;
        snprintf(text, sizeof(text), "conectando con %s", a->host);
    } else {
        mark = C_DIM;
        snprintf(text, sizeof(text), "sin conexión");
    }
    w = text_width(f_mono_small, text);
    x = r.x + r.w - w - 10;
    nk_fill_rect(canvas, nk_rect(x - 14, r.y + r.h / 2 - 3, 6, 6), 1, mark);
    text_in(ctx, nk_rect(x, r.y, w + 4, r.h), text, f_mono_small, C_MUTED, NK_TEXT_LEFT);
}

static void menu_bar(struct sdv_app *a, int width)
{
    struct nk_context *ctx = a->nk;
    struct sdv_settings *s = &a->set;
    struct nk_rect chip;
    struct nk_window *bar = nk_window_find(ctx, "menubar");

    /* While a dialog is open Nuklear marks the bar read-only; when the dialog closes the bar is
     * already the top window, so nothing clears the flag and its menus stop responding. */
    if (bar && a->dialog == SDV_DLG_NONE)
        bar->flags &= ~(nk_flags)NK_WINDOW_ROM;

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
    nk_fill_rect(nk_window_get_canvas(ctx), nk_rect(0, SDV_MENU_H - 1, (float)width, 1), 0, C_LINE);
    nk_menubar_begin(ctx);
    nk_layout_row_begin(ctx, NK_STATIC, 20, 6);

    nk_layout_row_push(ctx, MENU_W_FILE);
    if (nk_menu_begin_label(ctx, "Archivo", NK_TEXT_CENTERED, nk_vec2(220, 100))) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (a->streaming || is_connecting(a)) {
            if (nk_menu_item_label(ctx, "Desconectar", NK_TEXT_LEFT))
                app_disconnect(a, "Desconectado");
        } else {
            nk_label_colored(ctx, " Desconectar", NK_TEXT_LEFT, C_DIM);
        }
        if (nk_menu_item_label(ctx, "Salir", NK_TEXT_LEFT))
            a->quit = 1;
        nk_menu_end(ctx);
    }

    nk_layout_row_push(ctx, MENU_W_VIEW);
    if (nk_menu_begin_label(ctx, "Ver", NK_TEXT_CENTERED, nk_vec2(290, 100))) {
        int fs = a->fullscreen;
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (check(ctx, "Barra de estado", &s->status_bar))
            app_apply_settings(a, 0);
        if (check(ctx, "Pantalla completa      Alt+Enter", &fs))
            app_set_fullscreen(a, fs);
        nk_menu_end(ctx);
    }

    nk_layout_row_push(ctx, MENU_W_FUNC);
    if (nk_menu_begin_label(ctx, "Funciones", NK_TEXT_CENTERED, nk_vec2(310, 350))) {
        int reconnect = 0, changed = 0;
        nk_layout_row_dynamic(ctx, ROW, 1);
        reconnect |= check(ctx, "Pantalla", &s->display_enabled);
        changed |= check(ctx, "Teclado", &s->keyboard_enabled);
        separator(ctx);
        if (option(ctx, "Mouse remoto", &s->mouse_mode, SDV_MOUSE_REMOTE)) {
            changed = 1;
            app_toast(a, "Mouse remoto: haz clic en la imagen para controlar el PC");
        }
        if (option(ctx, "Mouse local (usar como táctil)", &s->mouse_mode, SDV_MOUSE_LOCAL_TOUCH)) {
            changed = 1;
            app_release_mouse(a);
            app_toast(a, "El mouse actúa como un dedo. Alt+Shift para salir");
        }
        separator(ctx);
        changed |= option(ctx, "Pantalla táctil (absoluto)", &s->touch_mode, SDV_TOUCH_ABSOLUTE);
        changed |= option(ctx, "Touchpad (relativo)", &s->touch_mode, SDV_TOUCH_RELATIVE);
        changed |= option(ctx, "Táctil apagado", &s->touch_mode, SDV_TOUCH_OFF);
        separator(ctx);
        reconnect |= check(ctx, "Audio (altavoz)", &s->audio_enabled);
        if (reconnect || changed)
            app_apply_settings(a, reconnect);
        nk_menu_end(ctx);
    }

    nk_layout_row_push(ctx, MENU_W_SETTINGS);
    if (nk_menu_begin_label(ctx, "Ajustes", NK_TEXT_CENTERED, nk_vec2(300, 320))) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (nk_menu_item_label(ctx, "Resolución…", NK_TEXT_LEFT))
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
    if (nk_menu_begin_label(ctx, "Ayuda", NK_TEXT_CENTERED, nk_vec2(290, 110))) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        if (nk_menu_item_label(ctx, "Manual de spacedesk", NK_TEXT_LEFT))
            SDL_OpenURL(MANUAL_URL);
        if (nk_menu_item_label(ctx, "Proyecto en GitHub", NK_TEXT_LEFT))
            SDL_OpenURL(PROJECT_URL);
        if (nk_menu_item_label(ctx, "Acerca de SpaceDesk Linux", NK_TEXT_LEFT))
            open_dialog(a, SDV_DLG_ABOUT);
        nk_menu_end(ctx);
    }

    nk_layout_row_push(ctx, (float)width - MENU_W_FILE - MENU_W_VIEW - MENU_W_FUNC - MENU_W_SETTINGS - MENU_W_HELP - 60);
    if (nk_widget(&chip, ctx) != NK_WIDGET_INVALID)
        status_text(a, chip);
    nk_layout_row_end(ctx);
    nk_menubar_end(ctx);
    nk_end(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_vec2(ctx);
    nk_style_pop_style_item(ctx);
}

/* ---------- connect screen ---------- */

/* One server row: name and address on the left, a Connect action on the right.
 * The whole row is clickable. */
static void server_row(struct sdv_app *a, const char *name, const char *address, float height)
{
    struct nk_context *ctx = a->nk;
    struct nk_command_buffer *canvas;
    struct nk_rect r, btn;
    int hover;

    nk_layout_row_dynamic(ctx, height, 1);
    if (nk_widget(&r, ctx) == NK_WIDGET_INVALID)
        return;
    canvas = nk_window_get_canvas(ctx);
    hover = a->dialog == SDV_DLG_NONE && nk_input_is_mouse_hovering_rect(&ctx->input, r);
    if (hover)
        nk_fill_rect(canvas, nk_rect(r.x - 10, r.y, r.w + 20, r.h), 4, C_RAISED);
    nk_fill_rect(canvas, nk_rect(r.x, r.y + r.h, r.w, 1), 0, C_LINE);

    btn = nk_rect(r.x + r.w - 104, r.y + (r.h - 30) / 2, 104, 30);
    if (name) {
        text_in(ctx, nk_rect(r.x, r.y + 7, r.w - 120, 22), name, f_bold, C_TEXT, NK_TEXT_LEFT);
        text_in(ctx, nk_rect(r.x, r.y + r.h - 25, r.w - 120, 18), address, f_mono_small, C_MUTED, NK_TEXT_LEFT);
    } else {
        text_in(ctx, nk_rect(r.x, r.y, r.w - 120, r.h), address, f_mono, C_TEXT, NK_TEXT_LEFT);
    }
    nk_fill_rect(canvas, btn, 4, hover ? C_ACCENT : C_CLEAR);
    nk_stroke_rect(canvas, btn, 4, 1, hover ? C_ACCENT : C_LINE);
    text_in(ctx, btn, "Conectar", f_small, hover ? C_ON_ACCENT : C_TEXT, NK_TEXT_CENTERED);

    if (hover && nk_input_mouse_clicked(&ctx->input, NK_BUTTON_LEFT, r))
        app_connect(a, address);
}

/* Section heading with an optional note on the right. */
static void list_heading(struct nk_context *ctx, const char *title, const char *note)
{
    struct nk_rect r;
    nk_layout_row_dynamic(ctx, 24, 1);
    if (nk_widget(&r, ctx) == NK_WIDGET_INVALID)
        return;
    text_in(ctx, r, title, f_bold, C_TEXT, NK_TEXT_LEFT);
    if (note)
        text_in(ctx, r, note, f_small, C_MUTED, NK_TEXT_RIGHT);
}

static void begin_plain(struct nk_context *ctx)
{
    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_color(C_BG));
    nk_style_push_float(ctx, &ctx->style.window.border, 0);
    nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(0, 0));
}

static void end_plain(struct nk_context *ctx)
{
    nk_style_pop_vec2(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_style_item(ctx);
}

static void connect_side(struct sdv_app *a, struct nk_rect r)
{
    static const char *const k_full[] = { "Alt", "Enter", NULL };
    static const char *const k_menu[] = { "Ctrl", "Alt", "M", NULL };
    static const char *const k_touch[] = { "Alt", "Shift", NULL };
    static const char *steps[] = {
        "En Windows, abre spacedesk y deja el servidor encendido.",
        "Conecta los dos equipos a la misma red o con un cable de red directo.",
        "Elige el PC en la lista o escribe su dirección.",
    };
    struct nk_context *ctx = a->nk;
    char line[160];

    begin_plain(ctx);
    if (nk_begin(ctx, "connect_side", r, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        list_heading(ctx, "Antes de conectar", NULL);
        gap(ctx, 4);
        for (int i = 0; i < 3; i++) {
            struct nk_rect n;
            nk_layout_row_template_begin(ctx, 34);
            nk_layout_row_template_push_static(ctx, 26);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_end(ctx);
            if (nk_widget(&n, ctx) != NK_WIDGET_INVALID) {
                snprintf(line, sizeof(line), "%d", i + 1);
                text_in(ctx, nk_rect(n.x, n.y + 1, n.w, 20), line, f_mono, C_ACCENT, NK_TEXT_LEFT);
            }
            nk_style_push_font(ctx, &f_small->handle);
            nk_label_colored_wrap(ctx, steps[i], C_MUTED);
            nk_style_pop_font(ctx);
        }
        gap(ctx, 22);
        list_heading(ctx, "Atajos", NULL);
        gap(ctx, 4);
        shortcut(ctx, k_full, "Pantalla completa");
        shortcut(ctx, k_menu, "Soltar el mouse y ver el menú");
        shortcut(ctx, k_touch, "Salir del modo táctil");
        gap(ctx, 22);
        snprintf(line, sizeof(line), "Versión %s  ·  sin afiliación con datronicsoft", SDV_VERSION);
        nk_layout_row_dynamic(ctx, 18, 1);
        label_font(ctx, line, f_small, C_DIM, NK_TEXT_LEFT);
    }
    nk_end(ctx);
    end_plain(ctx);
}

static void connect_screen(struct sdv_app *a, struct nk_rect area)
{
    struct nk_context *ctx = a->nk;
    struct sdv_server_info found[SDV_DISCOVERY_MAX];
    int n = a->disc ? sdv_discovery_list(a->disc, found, SDV_DISCOVERY_MAX) : 0;
    int wide = area.w >= 980;
    float margin = area.w >= 1200 ? 96 : area.w >= 800 ? 56 : 24;
    float main_w = wide ? (area.w - margin * 2) * 0.58f : area.w - margin * 2;
    struct nk_rect r;
    char line[256];

    /* SDV_DEMO: fixed sample server for documentation screenshots (no real names or addresses) */
    if (getenv("SDV_DEMO")) {
        n = 1;
        snprintf(found[0].name, sizeof(found[0].name), "PC-ESCRITORIO");
        snprintf(found[0].address, sizeof(found[0].address), "192.168.1.50");
    }
    if (main_w > 640)
        main_w = 640;

    r = nk_rect(area.x + margin, area.y + 44, main_w, area.h - 64);
    begin_plain(ctx);
    if (!nk_begin(ctx, "connect", r, NK_WINDOW_BACKGROUND)) {
        nk_end(ctx);
        end_plain(ctx);
        return;
    }

    nk_layout_row_dynamic(ctx, 38, 1);
    label_font(ctx, "SpaceDesk Linux", f_title, C_TEXT, NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 22, 1);
    label_font(ctx, "Usa este equipo como monitor extra de un PC con Windows.", f_regular, C_MUTED, NK_TEXT_LEFT);
    gap(ctx, 20);

    if (is_connecting(a)) {
        struct nk_rect bar;
        if (a->reconnect_at) {
            int secs = (int)((Sint32)(a->reconnect_at - SDL_GetTicks()) / 1000) + 1;
            snprintf(line, sizeof(line), "Reintentando con %s en %d s", a->host, secs < 0 ? 0 : secs);
        } else {
            snprintf(line, sizeof(line), "Conectando con %s%s", a->host, dots());
        }
        nk_layout_row_dynamic(ctx, 26, 1);
        label_font(ctx, line, f_bold, C_TEXT, NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 10, 1);
        if (nk_widget(&bar, ctx) != NK_WIDGET_INVALID)
            progress_line(ctx, nk_rect(bar.x, bar.y + 4, bar.w, 2));
        muted_wrap(ctx, "Windows mostrará un monitor nuevo en cuanto se establezca la conexión.", 22);
        if (a->message[0] && a->reconnect_at) {
            gap(ctx, 6);
            notice(ctx, a->message, C_ERR);
        }
        gap(ctx, 14);
        nk_layout_row_template_begin(ctx, 32);
        nk_layout_row_template_push_static(ctx, 110);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);
        if (nk_button_label_styled(ctx, &btn_secondary, "Cancelar"))
            app_disconnect(a, "");
        nk_end(ctx);
        end_plain(ctx);
        if (wide)
            connect_side(a, nk_rect(r.x + main_w + margin, r.y + 98, area.w - main_w - margin * 3, r.h - 98));
        return;
    }

    if (n)
        snprintf(line, sizeof(line), n == 1 ? "1 en la red" : "%d en la red", n);
    else
        snprintf(line, sizeof(line), "buscando%s", dots());
    list_heading(ctx, "Equipos con spacedesk", line);
    {
        struct nk_rect bar;
        nk_layout_row_dynamic(ctx, 6, 1);
        if (nk_widget(&bar, ctx) != NK_WIDGET_INVALID) {
            if (n)
                nk_fill_rect(nk_window_get_canvas(ctx), nk_rect(bar.x, bar.y + 4, bar.w, 1), 0, C_LINE);
            else
                progress_line(ctx, nk_rect(bar.x, bar.y + 3, bar.w, 2));
        }
    }
    if (!n)
        muted_wrap(ctx, "Aún no responde ningún PC. Comprueba que spacedesk esté activo en Windows y que el "
                        "firewall lo permita.", 40);
    for (int i = 0; i < n && i < 5; i++) {
        server_row(a, found[i].name, found[i].address, 54);
    }

    gap(ctx, 16);
    list_heading(ctx, "Conectar por dirección", NULL);
    gap(ctx, 2);
    nk_layout_row_template_begin(ctx, 34);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 104);
    nk_layout_row_template_end(ctx);
    {
        nk_flags ev;
        nk_style_push_font(ctx, &f_mono->handle);
        ev = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, a->address, sizeof(a->address),
                                            nk_filter_ascii);
        nk_style_pop_font(ctx);
        if ((nk_button_label_styled(ctx, &btn_primary, "Conectar") || (ev & NK_EDIT_COMMITED)) && a->address[0])
            app_connect(a, a->address);
    }
    muted_wrap(ctx, "IP o nombre del PC con Windows, por ejemplo 192.168.1.20", 20);

    if (a->set.recent[0][0]) {
        gap(ctx, 12);
        list_heading(ctx, "Recientes", NULL);
        gap(ctx, 2);
        for (int i = 0; i < SDV_MAX_RECENT && i < 3 && a->set.recent[i][0]; i++)
            server_row(a, NULL, a->set.recent[i], 38);
    }

    if (a->message[0]) {
        gap(ctx, 18);
        notice(ctx, a->message, C_ERR);
    }

    if (!wide) {
        static const char *const k_full[] = { "Alt", "Enter", NULL };
        static const char *const k_menu[] = { "Ctrl", "Alt", "M", NULL };
        gap(ctx, 22);
        shortcut(ctx, k_full, "Pantalla completa");
        shortcut(ctx, k_menu, "Soltar el mouse y ver el menú");
    }
    nk_end(ctx);
    end_plain(ctx);

    if (wide)
        connect_side(a, nk_rect(r.x + main_w + margin, r.y + 98, area.w - main_w - margin * 3, r.h - 98));
}

/* ---------- dialogs ---------- */

static void dialog_resolutions(struct sdv_app *a)
{
    static const int presets[][2] = {
        { 1280, 720 }, { 1280, 800 }, { 1366, 768 }, { 1600, 900 }, { 1920, 1080 },
        { 1920, 1200 }, { 2560, 1440 }, { 3840, 2160 }, { 1024, 768 },
    };
    static const char *names[] = {
        "1280 × 720", "1280 × 800", "1366 × 768", "1600 × 900", "1920 × 1080",
        "1920 × 1200", "2560 × 1440", "3840 × 2160", "1024 × 768",
    };
    struct nk_context *ctx = a->nk;
    struct sdv_settings *e = &a->edit;
    char label[128];
    int nw, nh, r, count = (int)(sizeof(presets) / sizeof(presets[0]));

    if (!begin_dialog(a, "Resolución", 440, e->custom_resolution ? 400 : 290)) {
        nk_end(ctx);
        return;
    }
    native_resolution(a, &nw, &nh);
    section_list(ctx, "Monitor extra en Windows");
    snprintf(label, sizeof(label), "La de esta pantalla (%d × %d)", nw, nh);
    option(ctx, label, &e->custom_resolution, 0);
    option(ctx, "Personalizada", &e->custom_resolution, 1);
    if (e->custom_resolution) {
        int sel = -1;
        for (int i = 0; i < count; i++)
            if (presets[i][0] == e->custom_width && presets[i][1] == e->custom_height)
                sel = i;
        nk_layout_row_dynamic(ctx, 30, 1);
        r = nk_combo(ctx, names, count, sel < 0 ? 0 : sel, 26, nk_vec2(240, 260));
        if (r != sel && (sel >= 0 || r != 0)) {
            e->custom_width = presets[r][0];
            e->custom_height = presets[r][1];
        }
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_property_int(ctx, "#Ancho:", 320, &e->custom_width, 7680, 2, 4);
        nk_property_int(ctx, "#Alto:", 240, &e->custom_height, 4320, 2, 4);
        if ((int64_t)e->custom_width * e->custom_height > (int64_t)nw * nh) {
            nk_layout_row_dynamic(ctx, 36, 1);
            nk_style_push_font(ctx, &f_small->handle);
            nk_label_colored_wrap(ctx, "Más grande que esta pantalla: no se ve más nítido y baja los cuadros por segundo.",
                                  C_ACCENT);
            nk_style_pop_font(ctx);
        }
    }
    muted_wrap(ctx, "Al aplicar, la sesión se reconecta.", 20);
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

    if (!begin_dialog(a, "Calidad y rendimiento", 500, 600)) {
        nk_end(ctx);
        return;
    }
    section(ctx, "Compresión");
    nk_layout_row_dynamic(ctx, ROW, 2);
    if (option(ctx, "Activada", &compression, 1))
        e->encoding = SDV_ENC_PREF_AUTO;
    if (option(ctx, "Desactivada (RGBA)", &compression, 0))
        e->encoding = SDV_ENC_PREF_OFF;
    if (!compression)
        notice(ctx, "Sin compresión hace falta una red muy rápida.", C_ACCENT);

    section(ctx, "Códec");
    if (compression) {
        nk_layout_row_dynamic(ctx, ROW, 1);
        option(ctx, "Automático", &e->encoding, SDV_ENC_PREF_AUTO);
        option(ctx, "H.264 (el más fluido)", &e->encoding, SDV_ENC_PREF_H264);
        option(ctx, "JPEG", &e->encoding, SDV_ENC_PREF_JPEG);
    } else {
        muted_wrap(ctx, "No aplica sin compresión.", 20);
    }

    section(ctx, "Calidad de imagen");
    nk_layout_row_template_begin(ctx, ROW);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 52);
    nk_layout_row_template_end(ctx);
    nk_slider_int(ctx, 1, &e->quality, 100, 1);
    snprintf(q, sizeof(q), "%d %%", e->quality);
    label_font(ctx, q, f_mono, C_TEXT, NK_TEXT_RIGHT);
    muted_wrap(ctx, "Más calidad usa más red. Con cable, 70–100 va bien.", 20);
    nk_layout_row_dynamic(ctx, ROW, 1);
    check(ctx, "Turbo: media resolución, más velocidad", &e->turbo);

    section(ctx, "Cuadros por segundo");
    nk_layout_row_dynamic(ctx, ROW, 4);
    option(ctx, "30", &e->fps, 30);
    option(ctx, "60", &e->fps, 60);
    option(ctx, "90", &e->fps, 90);
    option(ctx, "120", &e->fps, 120);

    r = dialog_buttons(ctx, "Aplicar");
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

    if (!begin_dialog(a, "Conexión automática", 440, 310)) {
        nk_end(ctx);
        return;
    }
    section_list(ctx, "Al abrir la aplicación");
    option(ctx, "Mostrar la pantalla de conexión", &e->autoconnect, SDV_AUTOCONNECT_OFF);
    option(ctx, "Conectar directamente a:", &e->autoconnect, SDV_AUTOCONNECT_ADDRESS);
    nk_layout_row_dynamic(ctx, 34, 1);
    nk_style_push_font(ctx, &f_mono->handle);
    nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, a->autoconnect_host, sizeof(a->autoconnect_host),
                                   nk_filter_ascii);
    nk_style_pop_font(ctx);
    muted_wrap(ctx, "Si la conexión se corta, se reintenta sola.", 20);
    r = dialog_buttons(ctx, "Aplicar");
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

    if (!begin_dialog(a, "Avanzado", 460, 360)) {
        nk_end(ctx);
        return;
    }
    section_list(ctx, "Imagen sin pérdidas");
    check(ctx, "YUV sin comprimir", &e->yuv);
    muted_wrap(ctx, "Necesita una red muy rápida (cable).", 20);
    section_list(ctx, "Decodificación");
    check(ctx, "H.264 por hardware (VA-API)", &e->hw_decode);
    muted_wrap(ctx, "Desactívalo solo si ves artefactos o la imagen no aparece.", 20);
    r = dialog_buttons(ctx, "Aplicar");
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
    nk_layout_row_dynamic(ctx, 20, 1);
    snprintf(line, sizeof(line), "versión %s", SDV_VERSION);
    label_font(ctx, line, f_mono_small, C_MUTED, NK_TEXT_LEFT);
    gap(ctx, 4);
    muted_wrap(ctx, "Visor libre para usar un equipo Linux como monitor extra de un PC con Windows que "
                    "ejecuta spacedesk. spacedesk es una marca de datronicsoft Inc.; este proyecto no está "
                    "afiliado.", 58);
    nk_layout_row_dynamic(ctx, 18, 1);
    snprintf(line, sizeof(line), "ID %s", a->device_id);
    label_font(ctx, line, f_mono_small, C_DIM, NK_TEXT_LEFT);
    gap(ctx, 10);
    nk_layout_row_template_begin(ctx, 32);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 104);
    nk_layout_row_template_push_static(ctx, 104);
    nk_layout_row_template_end(ctx);
    nk_spacing(ctx, 1);
    if (nk_button_label_styled(ctx, &btn_secondary, "GitHub"))
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
    snprintf(text, sizeof(text), "%s  >  %s    %d×%d    %3.0f fps    %5.1f Mbit/s    %s%s%s    recibido %s    enviado %s",
             st.local_addr, st.remote_addr, st.remote_w, st.remote_h, a->fps, a->mbps, sd_encoding_name(st.encoding),
             st.encoding == SD_ENC_H264 && st.h264_backend ? " " : "",
             st.encoding == SD_ENC_H264 && st.h264_backend ? st.h264_backend : "", rx, tx);
    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_color(C_BAR));
    nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(12, 3));
    nk_style_push_float(ctx, &ctx->style.window.rounding, 0);
    nk_style_push_float(ctx, &ctx->style.window.border, 0);
    if (nk_begin(ctx, "statusbar", nk_rect(0, (float)(height - SDV_STATUS_H), (float)width, SDV_STATUS_H),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT | NK_WINDOW_BACKGROUND)) {
        nk_fill_rect(nk_window_get_canvas(ctx), nk_rect(0, (float)(height - SDV_STATUS_H), (float)width, 1), 0, C_LINE);
        nk_layout_row_dynamic(ctx, 16, 1);
        label_font(ctx, text, f_mono_small, C_MUTED, NK_TEXT_LEFT);
    }
    nk_end(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_vec2(ctx);
    nk_style_pop_style_item(ctx);
}

/* Small floating panel: text, then optional key caps. Used for toasts and hints. */
static void floating(struct nk_context *ctx, const char *name, const char *text, const char *const *keys,
                     const char *after, float cx, float y)
{
    float w = text_width(f_small, text) + 32;
    if (keys) {
        for (int i = 0; keys[i]; i++)
            w += text_width(f_mono_small, keys[i]) + 12 + (i ? 14 : 10);
        w += after ? text_width(f_small, after) + 10 : 0;
    }
    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_color(nk_rgba(17, 18, 20, 235)));
    nk_style_push_float(ctx, &ctx->style.window.rounding, 4);
    nk_style_push_float(ctx, &ctx->style.window.border, 1);
    nk_style_push_color(ctx, &ctx->style.window.border_color, C_LINE);
    nk_style_push_vec2(ctx, &ctx->style.window.padding, nk_vec2(0, 0));
    if (nk_begin(ctx, name, nk_rect(cx - w / 2, y, w, 34), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT)) {
        struct nk_rect r;
        nk_layout_row_dynamic(ctx, 34, 1);
        if (nk_widget(&r, ctx) != NK_WIDGET_INVALID) {
            struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
            float x = r.x + 16;
            nk_fill_rect(canvas, nk_rect(r.x, r.y + 8, 2, r.h - 16), 0, C_ACCENT);
            text_in(ctx, nk_rect(x, r.y, w, r.h), text, f_small, C_TEXT, NK_TEXT_LEFT);
            x += text_width(f_small, text);
            if (keys) {
                x += 10;
                for (int i = 0; keys[i]; i++) {
                    if (i) {
                        text_in(ctx, nk_rect(x + 3, r.y, 10, r.h), "+", f_small, C_DIM, NK_TEXT_LEFT);
                        x += 14;
                    }
                    x += keycap(canvas, x, r.y + 7, keys[i]);
                }
                if (after)
                    text_in(ctx, nk_rect(x + 10, r.y, w, r.h), after, f_small, C_MUTED, NK_TEXT_LEFT);
            }
        }
    }
    nk_end(ctx);
    nk_style_pop_vec2(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_float(ctx);
    nk_style_pop_style_item(ctx);
}

/* ---------- frame ---------- */

void ui_frame(struct sdv_app *a)
{
    static const char *const k_release[] = { "Ctrl", "Alt", "M", NULL };
    int w, h, top, bottom;
    int show_menu, show_status = a->set.status_bar && a->streaming;
    struct nk_rect area;

    /* fullscreen: hide the menu bar again once the pointer has left it (or left the window) */
    if (a->fullscreen && a->menu_open && a->dialog == SDV_DLG_NONE) {
        struct nk_window *bar = nk_window_find(a->nk, "menubar");
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if ((SDL_GetMouseFocus() == a->win && my <= SDV_MENU_H + 6) || (bar && bar->popup.win && bar->popup.active))
            a->menu_until = SDL_GetTicks() + 800;
        else if (SDL_TICKS_PASSED(SDL_GetTicks(), a->menu_until))
            a->menu_open = 0;
    }
    show_menu = !a->fullscreen || a->menu_open;

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
        floating(a->nk, "hint", "Haz clic en la imagen para controlar el PC.", k_release, "lo suelta", (float)w / 2,
                 area.y + 12);

    /* dim everything behind an open dialog */
    if (a->dialog != SDV_DLG_NONE) {
        nk_style_push_style_item(a->nk, &a->nk->style.window.fixed_background, nk_style_item_color(nk_rgba(0, 0, 0, 140)));
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
        floating(a->nk, "toast", a->toast, NULL, NULL, (float)w / 2, (float)h - bottom - 60);
    if (show_menu)
        menu_bar(a, w);
}
