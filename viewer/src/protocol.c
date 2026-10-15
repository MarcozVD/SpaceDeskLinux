#include "protocol.h"

#include <string.h>

#define IDENT_FLAG_EXTENDED 0x01u
#define IDENT_DEVICE_ID_CHARS 39
#define IDENT_NAME_OFFSET 78

static void put_utf16(uint8_t *dst, size_t max_bytes, const char *s)
{
    size_t o = 0;
    for (; s && *s && o + 2 <= max_bytes; s++, o += 2) {
        unsigned char c = (unsigned char)*s;
        sd_put_u16(dst + o, c < 0x80 ? c : '?');
    }
}

static void put_names(uint8_t *payload, const char *device_id, const char *name)
{
    put_utf16(payload, (IDENT_DEVICE_ID_CHARS - 1) * 2, device_id);
    put_utf16(payload + IDENT_NAME_OFFSET, SD_IDENT_PAYLOAD_LEN - IDENT_NAME_OFFSET - 2, name);
}

/* Matches the identification of the official Windows viewer (see docs/re-log.md). */
void sd_build_identification(uint8_t *out, const struct sd_ident_params *p)
{
    uint8_t *h = out;
    int n = p->nres < 1 ? 1 : p->nres > SD_MAX_RESOLUTIONS ? SD_MAX_RESOLUTIONS : p->nres;

    memset(out, 0, SD_HEADER_LEN + SD_IDENT_PAYLOAD_LEN);
    sd_put_u32(h + 0, SD_T_IDENTIFICATION);
    sd_put_u32(h + 4, SD_IDENT_PAYLOAD_LEN);
    sd_put_u32(h + 8, SD_VERSION_MAJOR);
    sd_put_u32(h + 12, SD_VERSION_MINOR);
    sd_put_u32(h + 16, SD_CLIENT_REMOTE_MONITOR);
    sd_put_u32(h + 20, p->os_type);
    sd_put_u32(h + 24, p->encoding != SD_ENC_BGRX ? SD_ENC_JPEG : SD_ENC_BGRX); /* legacy field */
    sd_put_u32(h + 28, 2);
    sd_put_u32(h + 32, p->quality ? p->quality : 1);
    sd_put_u16(h + 36, p->encoding);
    sd_put_u16(h + 38, IDENT_FLAG_EXTENDED);
    sd_put_u16(h + 44, p->fps);
    sd_put_u16(h + 46, 4);
    sd_put_u32(h + 48, (uint32_t)n);
    for (int i = 0; i < n; i++) {
        sd_put_u32(h + 52 + 4 * i, p->res_w[i]);
        sd_put_u32(h + 88 + 4 * i, p->res_h[i]);
    }
    put_names(out + SD_HEADER_LEN, p->device_id, p->name);
}

void sd_build_speaker_identification(uint8_t *out, const char *device_id, const char *name)
{
    memset(out, 0, SD_HEADER_LEN + SD_IDENT_PAYLOAD_LEN);
    sd_put_u32(out + 0, SD_T_IDENTIFICATION);
    sd_put_u32(out + 4, SD_IDENT_PAYLOAD_LEN);
    sd_put_u32(out + 8, SD_VERSION_MAJOR);
    sd_put_u32(out + 12, SD_VERSION_MINOR);
    sd_put_u32(out + 16, SD_CLIENT_SPEAKER);
    sd_put_u32(out + 20, SD_OS_TYPE_VIEWER);
    put_names(out + SD_HEADER_LEN, device_id, name);
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
    /* the official viewer forwards the raw-input wheel delta as an unsigned 16-bit value */
    sd_put_u32(out + 16, (uint32_t)data & 0xFFFFu);
    sd_put_u32(out + 20, button_flags | ((dx || dy) ? SD_MOUSE_MOVE : 0));
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
                    uint32_t flags, uint32_t timestamp_ms)
{
    memset(out, 0, SD_HEADER_LEN);
    sd_put_u32(out + 0, SD_T_TOUCH);
    sd_put_u32(out + 8, (uint32_t)x);
    sd_put_u32(out + 12, (uint32_t)y);
    sd_put_u32(out + 16, client_w);
    sd_put_u32(out + 20, client_h);
    sd_put_u32(out + 24, flags);
    sd_put_u32(out + 28, timestamp_ms);
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

void sd_parse_audio(const uint8_t *h, struct sd_audio_format *fmt)
{
    fmt->channels = sd_get_u32(h + 8);
    fmt->rate = sd_get_u32(h + 12);
    fmt->bits = sd_get_u32(h + 16);
}

int sd_parse_discovery_reply(const uint8_t *buf, size_t len, struct sd_discovery_reply *r)
{
    size_t o = 0;

    if (len != SD_DISCOVERY_REPLY_LEN)
        return -1;
    memset(r, 0, sizeof(*r));
    /* UTF-16LE computer name in the first 256 bytes; keep it simple: Latin-1 subset */
    for (size_t i = 0; i + 1 < 256 && o + 3 < sizeof(r->name); i += 2) {
        unsigned c = (unsigned)buf[i] | (unsigned)buf[i + 1] << 8;
        if (!c)
            break;
        if (c < 0x80) {
            r->name[o++] = (char)c;
        } else if (c < 0x800) {
            r->name[o++] = (char)(0xC0 | (c >> 6));
            r->name[o++] = (char)(0x80 | (c & 0x3F));
        } else {
            r->name[o++] = '?';
        }
    }
    r->port = sd_get_u32(buf + 260);
    r->version_major = sd_get_u32(buf + 272);
    r->version_minor = sd_get_u32(buf + 276);
    if (!r->port)
        r->port = SD_PORT;
    return 0;
}

const char *sd_type_name(uint32_t type)
{
    static const char *names[] = {
        "Identification", "Ping", "FrameBuffer", "Visibility", "DisplaySettings",
        "Unused01", "EvtDisplaySettings", "FlowControlAck", "Error", "Rotation",
        "Mouse", "Keyboard", "Touch", "Pen", "AudioSpeaker",
    };
    return type < sizeof(names) / sizeof(names[0]) ? names[type] : "Unknown";
}

const char *sd_encoding_name(uint32_t enc)
{
    switch (enc) {
    case SD_ENC_BGRX: return "RGBA";
    case SD_ENC_YUV420: return "YUV";
    case SD_ENC_JPEG: return "JPEG";
    case SD_ENC_H264: return "H264";
    default: return "?";
    }
}
