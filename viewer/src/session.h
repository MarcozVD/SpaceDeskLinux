/* One display session with a spacedesk server: connect, identify, receive and decode
 * frames on a background thread, send input. */
#ifndef SDV_SESSION_H
#define SDV_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include <SDL.h>

#include "decoder.h"
#include "protocol.h"

enum sdv_session_state {
    SDV_STATE_CONNECTING = 0,
    SDV_STATE_STREAMING,
    SDV_STATE_CLOSED, /* by us or by the server */
    SDV_STATE_FAILED,
};

struct sdv_session_config {
    char host[128];
    int port;
    struct sd_ident_params ident;
    char device_id[64];
    char name[128];
    int allow_hw;
};

struct sdv_session_stats {
    uint64_t bytes_rx, bytes_tx;
    unsigned frames;
    uint32_t encoding;
    int remote_w, remote_h;
    char local_addr[64], remote_addr[160];
    const char *h264_backend;
    /* profiling (microseconds, accumulated) */
    uint64_t t_wait_us, t_decode_us, t_xfer_us;
    uint32_t max_frame_bytes;
};

struct sdv_session;

/* Starts connecting in the background. frame_event / state_event are SDL user event
 * types pushed when a new frame is ready or the state changes. */
struct sdv_session *sdv_session_start(const struct sdv_session_config *cfg,
                                      Uint32 frame_event, Uint32 state_event);
/* Sends Disconnect, stops the thread and frees everything. NULL is ignored. */
void sdv_session_stop(struct sdv_session *s);

enum sdv_session_state sdv_session_state(struct sdv_session *s, char *err, size_t errlen);
int sdv_session_send(struct sdv_session *s, const void *buf, size_t len);

/* Calls fn with the newest decoded frame if one arrived since the last call.
 * The image is only valid during the callback. Returns 1 if fn was called. */
int sdv_session_take_frame(struct sdv_session *s,
                           void (*fn)(void *user, const struct sdv_image *img), void *user);

/* Size in pixels the video is displayed at (lets the decoder scale on the GPU). */
void sdv_session_set_target(struct sdv_session *s, int w, int h);

void sdv_session_get_stats(struct sdv_session *s, struct sdv_session_stats *out);

#endif
