/* Frame decoding: H.264 (libavcodec, optional VA-API), JPEG (TurboJPEG), raw BGRX. */
#ifndef SDV_DECODER_H
#define SDV_DECODER_H

#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

enum sdv_pixfmt {
    SDV_FMT_NONE = 0,
    SDV_FMT_BGRX, /* 32 bpp, bytes B,G,R,X */
    SDV_FMT_NV12,
    SDV_FMT_I420,
};

struct sdv_image {
    enum sdv_pixfmt fmt;
    int width, height; /* pixels in plane[] */
    int src_w, src_h;  /* stream size (input coordinates); larger when scaled on the GPU */
    uint8_t *plane[3];
    int pitch[3];
    uint8_t *buf;
    size_t cap;
};

struct sdv_decoder;

struct sdv_decoder *sdv_decoder_new(int allow_hw);
void sdv_decoder_free(struct sdv_decoder *d);

/* Feed one FrameBuffer payload. Returns 1 when a complete frame was written to *out,
 * 0 when more data is needed (fragment) or the decoder produced no picture yet,
 * -1 on error. */
int sdv_decoder_feed(struct sdv_decoder *d, const struct sd_framebuffer *fb,
                     const uint8_t *data, size_t len, struct sdv_image *out);

/* Human readable description of the active H.264 path ("vaapi", "software", "none"). */
const char *sdv_decoder_h264_backend(const struct sdv_decoder *d);

/* Displayed size in pixels: hardware-decoded H.264 larger than this is scaled on the GPU. */
void sdv_decoder_set_target(struct sdv_decoder *d, int w, int h);

/* Accumulated time spent decoding and downloading/copying pictures (microseconds). */
void sdv_decoder_timing(const struct sdv_decoder *d, uint64_t *decode_us, uint64_t *xfer_us);

void sdv_image_free(struct sdv_image *img);

#endif
