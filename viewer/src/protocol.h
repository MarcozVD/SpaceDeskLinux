/* spacedesk wire protocol constants and packet builders.
 * Written from docs/protocol.md (clean-room). All integers little-endian. */
#ifndef SDV_PROTOCOL_H
#define SDV_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define SD_PORT 28252
#define SD_HEADER_LEN 128
#define SD_IDENT_PAYLOAD_LEN 334
#define SD_DISCONNECT_LEN 28
#define SD_VERSION_MAJOR 4
#define SD_VERSION_MINOR 8
#define SD_MAX_RESOLUTIONS 9

enum sd_type {
    SD_T_IDENTIFICATION = 0,
    SD_T_PING = 1,
    SD_T_FRAMEBUFFER = 2,
    SD_T_VISIBILITY = 3,
    SD_T_DISPLAY_SETTINGS = 4,
    SD_T_EVT_DISPLAY_SETTINGS = 6,
    SD_T_FLOW_ACK = 7,
    SD_T_ERROR = 8,
    SD_T_ROTATION = 9,
    SD_T_MOUSE = 10,
    SD_T_KEYBOARD = 11,
    SD_T_TOUCH = 12,
    SD_T_PEN = 13,
    SD_T_AUDIO_SPEAKER = 14,
};

enum sd_client_type {
    SD_CLIENT_REMOTE_MONITOR = 0,
    SD_CLIENT_SPEAKER = 3,
};

/* OS type reported by the official Windows viewer */
#define SD_OS_TYPE_VIEWER 1

enum sd_encoding {
    SD_ENC_BGRX = 0,
    SD_ENC_YUV420 = 1,
    SD_ENC_JPEG = 3,
    SD_ENC_H264 = 4,
    SD_ENC_AUTO = 5, /* server chooses hardware H.264 or software JPEG */
};

/* Win32 MOUSEEVENTF_* (button flags of the mouse packet) */
#define SD_MOUSE_MOVE 0x0001u
#define SD_MOUSE_LEFTDOWN 0x0002u
#define SD_MOUSE_LEFTUP 0x0004u
#define SD_MOUSE_RIGHTDOWN 0x0008u
#define SD_MOUSE_RIGHTUP 0x0010u
#define SD_MOUSE_MIDDLEDOWN 0x0020u
#define SD_MOUSE_MIDDLEUP 0x0040u
#define SD_MOUSE_WHEEL 0x0800u
#define SD_MOUSE_HWHEEL 0x1000u
#define SD_WHEEL_DELTA 120

/* Keyboard flags as sent by the official viewer */
#define SD_KEY_EXTENDED 0x0001u
#define SD_KEY_UP 0x0002u

/* Touch flags */
#define SD_TOUCH_ABSOLUTE 0x1u /* touchscreen; cleared = touchpad (relative) */
#define SD_TOUCH_DOWN 0x2u
#define SD_TOUCH_UP 0x4u

struct sd_ident_params {
    uint32_t os_type;
    uint16_t encoding;
    uint32_t quality;
    uint16_t fps;
    int nres; /* 1..SD_MAX_RESOLUTIONS, first entry is the preferred one */
    uint32_t res_w[SD_MAX_RESOLUTIONS];
    uint32_t res_h[SD_MAX_RESOLUTIONS];
    const char *device_id; /* ASCII GUID */
    const char *name;
};

struct sd_framebuffer {
    uint32_t width, height, pitch, format;
    uint32_t x1, y1, x2, y2;
    uint32_t encoding;
    uint32_t fragment_count;
    uint32_t is_fragment;
};

struct sd_audio_format {
    uint32_t channels, rate, bits;
};

#define SD_DISCOVERY_REQUEST "SPACEDESK-NET-CLIENT"
#define SD_DISCOVERY_REPLY_LEN 308

struct sd_discovery_reply {
    char name[128]; /* server computer name (UTF-8 from UTF-16) */
    uint32_t port;
    uint32_t version_major, version_minor;
};

static inline uint32_t sd_get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline void sd_put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline void sd_put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

/* out must hold SD_HEADER_LEN + SD_IDENT_PAYLOAD_LEN bytes */
void sd_build_identification(uint8_t *out, const struct sd_ident_params *p);
void sd_build_speaker_identification(uint8_t *out, const char *device_id, const char *name);
/* out must hold SD_HEADER_LEN bytes */
void sd_build_flow_ack(uint8_t *out);
/* dx/dy are relative motion; data is the wheel delta */
void sd_build_mouse(uint8_t *out, int32_t dx, int32_t dy, int32_t data, uint32_t button_flags);
void sd_build_keyboard(uint8_t *out, uint32_t vk, uint32_t scancode, uint32_t flags);
void sd_build_touch(uint8_t *out, int32_t x, int32_t y, uint32_t client_w, uint32_t client_h,
                    uint32_t flags, uint32_t timestamp_ms);
/* out must hold SD_DISCONNECT_LEN bytes */
void sd_build_disconnect(uint8_t *out);

void sd_parse_framebuffer(const uint8_t *hdr, struct sd_framebuffer *fb);
void sd_parse_audio(const uint8_t *hdr, struct sd_audio_format *fmt);
/* Returns 0 if buf is a discovery reply. */
int sd_parse_discovery_reply(const uint8_t *buf, size_t len, struct sd_discovery_reply *r);
const char *sd_type_name(uint32_t type);
const char *sd_encoding_name(uint32_t enc);

#endif
