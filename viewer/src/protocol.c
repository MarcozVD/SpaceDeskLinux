#include "protocol.h"

#include <string.h>

#define IDENT_FLAG_EXTENDED 0x01u

static void put_utf16(uint8_t *dst, size_t max_bytes, const char *s)
{
    size_t o = 0;
    for (; s && *s && o + 2 <= max_bytes; s++, o += 2) {
        unsigned char c = (unsigned char)*s;
        sd_put_u16(dst + o, c < 0x80 ? c : '?');
    }
}

void sd_build_identification(uint8_t *out, const struct sd_ident_params *p)
{
    uint8_t *h = out;
    uint8_t *pl = out + SD_HEADER_LEN;

    memset(out, 0, SD_HEADER_LEN + SD_IDENT_PAYLOAD_LEN);
    sd_put_u32(h + 0, SD_T_IDENTIFICATION);
    sd_put_u32(h + 4, SD_IDENT_PAYLOAD_LEN);
    sd_put_u32(h + 8, SD_VERSION_MAJOR);
    sd_put_u32(h + 12, SD_VERSION_MINOR);
    sd_put_u32(h + 16, p->client_type);
    sd_put_u32(h + 20, p->os_type);
    sd_put_u32(h + 24, SD_ENC_JPEG); /* legacy field, ignored in extended mode */
    sd_put_u32(h + 32, p->quality ? p->quality : 1);
    sd_put_u16(h + 36, p->encoding);
    h[38] = IDENT_FLAG_EXTENDED;
    sd_put_u16(h + 44, p->fps);
    sd_put_u32(h + 48, 1); /* resolution mode: client resolution */
    sd_put_u32(h + 52, p->width);
    sd_put_u32(h + 88, p->height);

    put_utf16(pl, 76, p->device_id);
    put_utf16(pl + 78, SD_IDENT_PAYLOAD_LEN - 78 - 2, p->name);
}

void sd_build_flow_ack(uint8_t *out)
{
    memset(out, 0, SD_HEADER_LEN);
    sd_put_u32(out, SD_T_FLOW_ACK);
}

void sd_build_mouse(uint8_t *out, int32_t dx, int32_t dy, int32_t data, uint32_t button_flags)
{
    memset(out, 0, SD_HEADER_LEN);
    sd_put_u32(out + 0, SD_T_MOUSE);
    sd_put_u32(out + 8, (uint32_t)dx);
    sd_put_u32(out + 12, (uint32_t)dy);
    sd_put_u32(out + 16, (uint32_t)data);
    sd_put_u32(out + 20, button_flags);
}

void sd_build_keyboard(uint8_t *out, uint32_t vk, uint32_t scancode, uint32_t flags)
{
    memset(out, 0, SD_HEADER_LEN);
    sd_put_u32(out + 0, SD_T_KEYBOARD);
    sd_put_u32(out + 8, vk);
    sd_put_u32(out + 12, scancode);
    sd_put_u32(out + 16, flags);
}

void sd_build_touch(uint8_t *out, int32_t x, int32_t y, uint32_t client_w, uint32_t client_h,
                    uint32_t flags, uint32_t contact, uint32_t timestamp_ms)
{
    memset(out, 0, SD_HEADER_LEN);
    sd_put_u32(out + 0, SD_T_TOUCH);
    sd_put_u32(out + 8, (uint32_t)x);
    sd_put_u32(out + 12, (uint32_t)y);
    sd_put_u32(out + 16, client_w);
    sd_put_u32(out + 20, client_h);
    sd_put_u32(out + 24, flags);
    sd_put_u32(out + 28, timestamp_ms);
    sd_put_u32(out + 32, contact);
}

void sd_build_disconnect(uint8_t *out)
{
    memset(out, 0, SD_DISCONNECT_LEN);
    sd_put_u32(out, SD_T_ERROR);
}

void sd_parse_framebuffer(const uint8_t *h, struct sd_framebuffer *fb)
{
    fb->width = sd_get_u32(h + 8);
    fb->height = sd_get_u32(h + 12);
    fb->pitch = sd_get_u32(h + 16);
    fb->format = sd_get_u32(h + 20);
    fb->x1 = sd_get_u32(h + 24);
    fb->y1 = sd_get_u32(h + 28);
    fb->x2 = sd_get_u32(h + 32);
    fb->y2 = sd_get_u32(h + 36);
    fb->encoding = sd_get_u32(h + 40);
    fb->fragment_count = sd_get_u32(h + 60);
    fb->is_fragment = sd_get_u32(h + 64);
}

const char *sd_type_name(uint32_t type)
{
    static const char *names[] = {
        "Identification", "Ping", "FrameBuffer", "Visibility", "DisplaySettings",
        "Unused01", "EvtDisplaySettings", "FlowControlAck", "Error", "Rotation",
        "Mouse", "Keyboard", "Touch", "Pen",
    };
    return type < sizeof(names) / sizeof(names[0]) ? names[type] : "Unknown";
}
