#include "decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <turbojpeg.h>

struct sdv_decoder {
    int allow_hw;

    /* H.264 */
    AVCodecContext *avctx;
    AVBufferRef *hwdev;
    AVPacket *pkt;
    AVFrame *frame;
    AVFrame *swframe;
    uint8_t *pktbuf;
    size_t pktcap;
    const char *backend;

    /* Fragmented formats (JPEG, BGRX): assembled into a BGRX canvas */
    tjhandle tj;
    uint8_t *canvas;
    int canvas_w, canvas_h;
    unsigned fragments;
};

static int image_reserve(struct sdv_image *img, enum sdv_pixfmt fmt, int w, int h)
{
    size_t need;
    int cw = (w + 1) / 2, ch = (h + 1) / 2;

    switch (fmt) {
    case SDV_FMT_BGRX: need = (size_t)w * h * 4; break;
    case SDV_FMT_NV12: need = (size_t)w * h + (size_t)cw * 2 * ch; break;
    case SDV_FMT_I420: need = (size_t)w * h + (size_t)cw * ch * 2; break;
    default: return -1;
    }
    if (img->cap < need) {
        uint8_t *nb = realloc(img->buf, need);
        if (!nb)
            return -1;
        img->buf = nb;
        img->cap = need;
    }
    img->fmt = fmt;
    img->width = w;
    img->height = h;
    memset(img->plane, 0, sizeof(img->plane));
    memset(img->pitch, 0, sizeof(img->pitch));
    img->plane[0] = img->buf;
    switch (fmt) {
    case SDV_FMT_BGRX:
        img->pitch[0] = w * 4;
        break;
    case SDV_FMT_NV12:
        img->pitch[0] = w;
        img->plane[1] = img->buf + (size_t)w * h;
        img->pitch[1] = cw * 2;
        break;
    case SDV_FMT_I420:
        img->pitch[0] = w;
        img->plane[1] = img->buf + (size_t)w * h;
        img->pitch[1] = cw;
        img->plane[2] = img->plane[1] + (size_t)cw * ch;
        img->pitch[2] = cw;
        break;
    default:
        break;
    }
    return 0;
}

void sdv_image_free(struct sdv_image *img)
{
    free(img->buf);
    memset(img, 0, sizeof(*img));
}

static void copy_plane(uint8_t *dst, int dpitch, const uint8_t *src, int spitch, int bytes, int rows)
{
    for (int y = 0; y < rows; y++)
        memcpy(dst + (size_t)y * dpitch, src + (size_t)y * spitch, (size_t)bytes);
}

/* ---------- H.264 ---------- */

static enum AVPixelFormat pick_format(AVCodecContext *ctx, const enum AVPixelFormat *fmts)
{
    (void)ctx;
    for (const enum AVPixelFormat *p = fmts; *p != AV_PIX_FMT_NONE; p++)
        if (*p == AV_PIX_FMT_VAAPI)
            return *p;
    /* No VA-API surface format offered: take the first software format. */
    for (const enum AVPixelFormat *p = fmts; *p != AV_PIX_FMT_NONE; p++) {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);
        if (desc && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

static int h264_open(struct sdv_decoder *d, int try_hw)
{
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "decoder: libavcodec has no H.264 decoder\n");
        return -1;
    }
    d->avctx = avcodec_alloc_context3(codec);
    if (!d->avctx)
        return -1;
    d->avctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    d->avctx->flags2 |= AV_CODEC_FLAG2_FAST;

    d->backend = "software";
    if (try_hw && av_hwdevice_ctx_create(&d->hwdev, AV_HWDEVICE_TYPE_VAAPI, NULL, NULL, 0) == 0) {
        d->avctx->hw_device_ctx = av_buffer_ref(d->hwdev);
        d->avctx->get_format = pick_format;
        d->backend = "vaapi";
    } else {
        /* Slice threads keep latency at one frame, unlike frame threads. */
        d->avctx->thread_type = FF_THREAD_SLICE;
        d->avctx->thread_count = 0;
    }

    if (avcodec_open2(d->avctx, codec, NULL) < 0) {
        fprintf(stderr, "decoder: avcodec_open2 failed (%s)\n", d->backend);
        avcodec_free_context(&d->avctx);
        av_buffer_unref(&d->hwdev);
        return -1;
    }
    if (!d->pkt)
        d->pkt = av_packet_alloc();
    if (!d->frame)
        d->frame = av_frame_alloc();
    if (!d->swframe)
        d->swframe = av_frame_alloc();
    if (!d->pkt || !d->frame || !d->swframe)
        return -1;
    fprintf(stderr, "decoder: H.264 via %s\n", d->backend);
    return 0;
}

static void h264_close(struct sdv_decoder *d)
{
    avcodec_free_context(&d->avctx);
    av_buffer_unref(&d->hwdev);
}

static int frame_to_image(const AVFrame *f, struct sdv_image *out)
{
    int w = f->width, h = f->height;
    int cw = (w + 1) / 2, ch = (h + 1) / 2;

    switch (f->format) {
    case AV_PIX_FMT_NV12:
        if (image_reserve(out, SDV_FMT_NV12, w, h) < 0)
            return -1;
        copy_plane(out->plane[0], out->pitch[0], f->data[0], f->linesize[0], w, h);
        copy_plane(out->plane[1], out->pitch[1], f->data[1], f->linesize[1], cw * 2, ch);
        return 0;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
        if (image_reserve(out, SDV_FMT_I420, w, h) < 0)
            return -1;
        copy_plane(out->plane[0], out->pitch[0], f->data[0], f->linesize[0], w, h);
        copy_plane(out->plane[1], out->pitch[1], f->data[1], f->linesize[1], cw, ch);
        copy_plane(out->plane[2], out->pitch[2], f->data[2], f->linesize[2], cw, ch);
        return 0;
    default:
        fprintf(stderr, "decoder: unsupported decoded format %s\n",
                av_get_pix_fmt_name((enum AVPixelFormat)f->format));
        return -1;
    }
}

static int h264_feed(struct sdv_decoder *d, const uint8_t *data, size_t len, struct sdv_image *out)
{
    int got = 0, ret;

    if (!d->avctx && h264_open(d, d->allow_hw) < 0) {
        if (!d->allow_hw || h264_open(d, 0) < 0)
            return -1;
    }

    /* libavcodec requires zeroed padding after the bitstream. */
    if (d->pktcap < len + AV_INPUT_BUFFER_PADDING_SIZE) {
        uint8_t *nb = realloc(d->pktbuf, len + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!nb)
            return -1;
        d->pktbuf = nb;
        d->pktcap = len + AV_INPUT_BUFFER_PADDING_SIZE;
    }
    memcpy(d->pktbuf, data, len);
    memset(d->pktbuf + len, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    d->pkt->data = d->pktbuf;
    d->pkt->size = (int)len;

    ret = avcodec_send_packet(d->avctx, d->pkt);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        fprintf(stderr, "decoder: send_packet failed (%d)\n", ret);
        return 0; /* drop this access unit, keep the session */
    }

    while ((ret = avcodec_receive_frame(d->avctx, d->frame)) == 0) {
        const AVFrame *src = d->frame;
        if (d->frame->format == AV_PIX_FMT_VAAPI) {
            av_frame_unref(d->swframe);
            if (av_hwframe_transfer_data(d->swframe, d->frame, 0) < 0) {
                fprintf(stderr, "decoder: VA-API surface download failed, switching to software\n");
                av_frame_unref(d->frame);
                h264_close(d);
                d->allow_hw = 0;
                return 0;
            }
            d->swframe->width = d->frame->width;
            d->swframe->height = d->frame->height;
            src = d->swframe;
        }
        if (frame_to_image(src, out) == 0)
            got = 1;
        av_frame_unref(d->frame);
    }
    return got;
}

/* ---------- JPEG / BGRX fragments ---------- */

static int canvas_reserve(struct sdv_decoder *d, int w, int h)
{
    if (d->canvas && d->canvas_w == w && d->canvas_h == h)
        return 0;
    free(d->canvas);
    d->canvas = calloc((size_t)w * h, 4);
    d->canvas_w = d->canvas ? w : 0;
    d->canvas_h = d->canvas ? h : 0;
    d->fragments = 0;
    return d->canvas ? 0 : -1;
}

static int rect_valid(const struct sd_framebuffer *fb, int cw, int ch)
{
    return fb->x2 > fb->x1 && fb->y2 > fb->y1 && fb->x2 <= (uint32_t)cw && fb->y2 <= (uint32_t)ch;
}

static int fragment_done(struct sdv_decoder *d, const struct sd_framebuffer *fb, struct sdv_image *out)
{
    unsigned need = fb->is_fragment ? (fb->fragment_count ? fb->fragment_count : 1) : 1;

    if (++d->fragments < need)
        return 0;
    d->fragments = 0;
    if (image_reserve(out, SDV_FMT_BGRX, d->canvas_w, d->canvas_h) < 0)
        return -1;
    memcpy(out->plane[0], d->canvas, (size_t)d->canvas_w * d->canvas_h * 4);
    return 1;
}

static int jpeg_feed(struct sdv_decoder *d, const struct sd_framebuffer *fb,
                     const uint8_t *data, size_t len, struct sdv_image *out)
{
    int jw, jh, subsamp, cs;
    int pitch;

    if (canvas_reserve(d, (int)fb->width, (int)fb->height) < 0)
        return -1;
    if (!d->tj && !(d->tj = tjInitDecompress()))
        return -1;
    /* A bad fragment still counts toward the frame so the flow-control ack is sent. */
    if (!rect_valid(fb, d->canvas_w, d->canvas_h))
        return fragment_done(d, fb, out);
    if (tjDecompressHeader3(d->tj, data, (unsigned long)len, &jw, &jh, &subsamp, &cs) < 0) {
        fprintf(stderr, "decoder: bad JPEG header: %s\n", tjGetErrorStr2(d->tj));
        return fragment_done(d, fb, out);
    }
    if ((uint32_t)jw > fb->x2 - fb->x1 || (uint32_t)jh > fb->y2 - fb->y1)
        return fragment_done(d, fb, out);
    pitch = d->canvas_w * 4;
    if (tjDecompress2(d->tj, data, (unsigned long)len,
                      d->canvas + (size_t)fb->y1 * pitch + (size_t)fb->x1 * 4,
                      jw, pitch, jh, TJPF_BGRX, TJFLAG_FASTDCT) < 0)
        fprintf(stderr, "decoder: JPEG decode: %s\n", tjGetErrorStr2(d->tj));
    return fragment_done(d, fb, out);
}

static int bgrx_feed(struct sdv_decoder *d, const struct sd_framebuffer *fb,
                     const uint8_t *data, size_t len, struct sdv_image *out)
{
    int rw, rh, pitch;

    if (canvas_reserve(d, (int)fb->width, (int)fb->height) < 0)
        return -1;
    if (!rect_valid(fb, d->canvas_w, d->canvas_h))
        return fragment_done(d, fb, out);
    rw = (int)(fb->x2 - fb->x1);
    rh = (int)(fb->y2 - fb->y1);
    if (len < (size_t)rw * rh * 4)
        return fragment_done(d, fb, out);
    pitch = d->canvas_w * 4;
    copy_plane(d->canvas + (size_t)fb->y1 * pitch + (size_t)fb->x1 * 4, pitch,
               data, rw * 4, rw * 4, rh);
    return fragment_done(d, fb, out);
}

/* Raw 4:2:0 frame, full size, planar (layout assumed I420, see docs/protocol.md). */
static int yuv_feed(const struct sd_framebuffer *fb, const uint8_t *data, size_t len, struct sdv_image *out)
{
    int w = (int)fb->width, h = (int)fb->height;
    int cw = (w + 1) / 2, ch = (h + 1) / 2;
    size_t ysz = (size_t)w * h, csz = (size_t)cw * ch;

    if (w <= 0 || h <= 0 || len < ysz + 2 * csz)
        return 0;
    if (image_reserve(out, SDV_FMT_I420, w, h) < 0)
        return -1;
    memcpy(out->plane[0], data, ysz);
    memcpy(out->plane[1], data + ysz, csz);
    memcpy(out->plane[2], data + ysz + csz, csz);
    return 1;
}

/* ---------- public ---------- */

struct sdv_decoder *sdv_decoder_new(int allow_hw)
{
    struct sdv_decoder *d = calloc(1, sizeof(*d));
    if (d) {
        d->allow_hw = allow_hw;
        d->backend = "none";
    }
    return d;
}

void sdv_decoder_free(struct sdv_decoder *d)
{
    if (!d)
        return;
    h264_close(d);
    av_packet_free(&d->pkt);
    av_frame_free(&d->frame);
    av_frame_free(&d->swframe);
    free(d->pktbuf);
    if (d->tj)
        tjDestroy(d->tj);
    free(d->canvas);
    free(d);
}

int sdv_decoder_feed(struct sdv_decoder *d, const struct sd_framebuffer *fb,
                     const uint8_t *data, size_t len, struct sdv_image *out)
{
    switch (fb->encoding) {
    case SD_ENC_H264:
        return h264_feed(d, data, len, out);
    case SD_ENC_JPEG:
        return jpeg_feed(d, fb, data, len, out);
    case SD_ENC_BGRX:
        return bgrx_feed(d, fb, data, len, out);
    case SD_ENC_YUV420:
        return yuv_feed(fb, data, len, out);
    default:
        fprintf(stderr, "decoder: unsupported encoding %u\n", fb->encoding);
        return -1;
    }
}

const char *sdv_decoder_h264_backend(const struct sdv_decoder *d)
{
    return d->backend;
}
