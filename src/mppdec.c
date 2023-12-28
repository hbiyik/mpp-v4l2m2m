/*
 * mppdec.c
 *
 *  Created on: Dec 26, 2023
 *      Author: boogie
 */
#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cusedev.h"
#include "mppdec.h"

static struct rkmpp_fmt rkmpp_dec_fmts[] = {
    {
        .name = "4:2:0 1 plane Y/CbCr",
        .fourcc = V4L2_PIX_FMT_NV12,
        .num_planes = 1,
        .type = MPP_VIDEO_CodingUnused,
        .format = MPP_FMT_YUV420SP,
        .depth = { 12 },
    },
    {
        .name = "AV1",
        .fourcc = V4L2_PIX_FMT_AV1,
        .num_planes = 1,
        .type = MPP_VIDEO_CodingAV1,
        .format = MPP_FMT_BUTT,
        .frmsize = {
            .min_width = 48,
            .max_width = 7680,
            .step_width = RKMPP_MB_DIM,
            .min_height = 48,
            .max_height = 4320,
            .step_height = RKMPP_MB_DIM,
        },
    },
    {
        .name = "H.265",
        .fourcc = V4L2_PIX_FMT_HEVC,
        .num_planes = 1,
        .type = MPP_VIDEO_CodingHEVC,
        .format = MPP_FMT_BUTT,
        .frmsize = {
            .min_width = 48,
            .max_width = 3840,
            .step_width = RKMPP_MB_DIM,
            .min_height = 48,
            .max_height = 2160,
            .step_height = RKMPP_MB_DIM,
        },
    },
    {
        .name = "H.264",
        .fourcc = V4L2_PIX_FMT_H264,
        .num_planes = 1,
        .type = MPP_VIDEO_CodingAVC,
        .format = MPP_FMT_BUTT,
        .frmsize = {
            .min_width = 48,
            .max_width = 3840,
            .step_width = RKMPP_MB_DIM,
            .min_height = 48,
            .max_height = 2160,
            .step_height = RKMPP_MB_DIM,
        },
    },
    {
        .name = "VP8",
        .fourcc = V4L2_PIX_FMT_VP8,
        .num_planes = 1,
        .type = MPP_VIDEO_CodingVP8,
        .format = MPP_FMT_BUTT,
        .frmsize = {
            .min_width = 48,
            .max_width = 3840,
            .step_width = RKMPP_MB_DIM,
            .min_height = 48,
            .max_height = 2160,
            .step_height = RKMPP_MB_DIM,
        },
    },
    {
        .name = "VP9",
        .fourcc = V4L2_PIX_FMT_VP9,
        .num_planes = 1,
        .type = MPP_VIDEO_CodingVP9,
        .format = MPP_FMT_BUTT,
        .frmsize = {
            .min_width = 48,
            .max_width = 3840,
            .step_width = RKMPP_SB_DIM,
            .min_height = 48,
            .max_height = 2176,
            .step_height = RKMPP_SB_DIM,
        },
    },
};

static void rkmpp_put_packets(struct rkmpp_dec_context *dec) {
    struct rkmpp_context *ctx = dec->ctx;
    struct rkmpp_buffer *rkmpp_buffer;
    MppPacket packet;
    MPP_RET ret;
    bool is_eos;

    ENTER();

    pthread_mutex_lock(&ctx->output.queue_mutex);
    while (!TAILQ_EMPTY(&ctx->output.pending_buffers)) {
        rkmpp_buffer = TAILQ_FIRST(&ctx->output.pending_buffers);

        mpp_packet_init(&packet, mpp_buffer_get_ptr(rkmpp_buffer->rkmpp_buf), rkmpp_buffer->bytesused);
        mpp_packet_set_pts(packet, rkmpp_buffer->timestamp);

        // TODO: Support start/stop decode cmd
        /* The chromium uses -2 as special flush timestamp. */
        is_eos = rkmpp_buffer->timestamp == (uint64_t) -2000000;
        if (is_eos)
            mpp_packet_set_eos(packet);

        ret = ctx->mpi->decode_put_packet(ctx->mpp, packet);
        mpp_packet_deinit(&packet);

        if (ret != MPP_OK)
            break;

        TAILQ_REMOVE(&ctx->output.pending_buffers, rkmpp_buffer, entry);
        rkmpp_buffer_clr_pending(rkmpp_buffer);

        /* Hold eos packet until eos frame received(flushed) */
        if (is_eos) {
            LOGV(1, "hold eos packet: %d\n", rkmpp_buffer->index);
            dec->eos_packet = rkmpp_buffer;
            break;
        }

        LOGV(3, "put packet: %d(%" PRIu64 ") len=%d\n",
                rkmpp_buffer->index, rkmpp_buffer->timestamp,
                rkmpp_buffer->bytesused);

        rkmpp_buffer->bytesused = 0;

        LOGV(3, "return packet: %d\n", rkmpp_buffer->index);

        TAILQ_INSERT_TAIL(&ctx->output.avail_buffers, rkmpp_buffer, entry);
        rkmpp_buffer_set_available(rkmpp_buffer);
    }
    pthread_mutex_unlock(&ctx->output.queue_mutex);

    LEAVE();
}

/* Feed all available frames to mpp */
static void rkmpp_put_frames(struct rkmpp_dec_context *dec) {
    struct rkmpp_context *ctx = dec->ctx;
    struct rkmpp_buffer *rkmpp_buffer;

    ENTER();

    pthread_mutex_lock(&ctx->capture.queue_mutex);
    while (!TAILQ_EMPTY(&ctx->capture.pending_buffers)) {
        rkmpp_buffer = TAILQ_FIRST(&ctx->capture.pending_buffers);
        TAILQ_REMOVE(&ctx->capture.pending_buffers, rkmpp_buffer, entry);
        rkmpp_buffer_clr_pending(rkmpp_buffer);

        LOGV(3, "put frame: %d fd: %d\n", rkmpp_buffer->index, rkmpp_buffer->fd);

        mpp_buffer_put(rkmpp_buffer->rkmpp_buf);
        rkmpp_buffer_clr_locked(rkmpp_buffer);
    }
    pthread_mutex_unlock(&ctx->capture.queue_mutex);

    LEAVE();
}

static void rkmpp_apply_info_change(struct rkmpp_dec_context *dec, MppFrame frame) {
    struct rkmpp_context *ctx = dec->ctx;
    struct rkmpp_video_info video_info;

    ENTER();

    memcpy((void*) &video_info, (void*) &dec->video_info, sizeof(video_info));

    video_info.mpp_format = mpp_frame_get_fmt(frame);
    video_info.width = mpp_frame_get_width(frame);
    video_info.height = mpp_frame_get_height(frame);
    video_info.hor_stride = mpp_frame_get_hor_stride(frame);
    video_info.ver_stride = mpp_frame_get_ver_stride(frame);
    video_info.size = mpp_frame_get_buf_size(frame);
    video_info.valid = true;

    if (!memcmp((void*) &video_info, (void*) &dec->video_info, sizeof(video_info))) {
        LOGV(1, "ignore unchanged frame info\n");

        ctx->mpi->control(ctx->mpp, MPP_DEC_SET_INFO_CHANGE_READY,
        NULL);
        return;
    }

    dec->video_info = video_info;
    dec->video_info.dirty = true;
    dec->video_info.event = dec->event_subscribed;

    LOGV(1, "frame info changed: %dx%d(%dx%d:%d), mpp format(%d)\n",
            dec->video_info.width, dec->video_info.height,
            dec->video_info.hor_stride, dec->video_info.ver_stride,
            dec->video_info.size, dec->video_info.mpp_format);

    /*
     * Use ver_stride as new height, the visible rect would be returned
     * in g_selection.
     */
    ctx->capture.format.num_planes = 1;
    ctx->capture.format.width = dec->video_info.hor_stride;
    ctx->capture.format.height = dec->video_info.ver_stride;
    ctx->capture.format.plane_fmt[0].bytesperline = dec->video_info.hor_stride;
    ctx->capture.format.plane_fmt[0].sizeimage = dec->video_info.size;

    assert(dec->video_info.mpp_format == MPP_FMT_YUV420SP);
    ctx->capture.format.pixelformat = V4L2_PIX_FMT_NV12;

    LEAVE();
}

static void *decoder_thread_fn(void *data){
    struct rkmpp_dec_context *dec = data;
    struct rkmpp_context *ctx = dec->ctx;
    struct rkmpp_buffer *rkmpp_buffer;
    MppFrame frame;
    MppBuffer buffer;
    MPP_RET ret;
    int index;

    ENTER();

    LOGV(1, "ctx(%p): starting decoder thread\n", (void*) ctx);

    while (1) {
        pthread_mutex_lock(&dec->decoder_mutex);

        while (!dec->mpp_streaming)
            pthread_cond_wait(&dec->decoder_cond, &dec->decoder_mutex);

        /* Feed available packets and frames to mpp */
        rkmpp_put_packets(dec);
        rkmpp_put_frames(dec);

        frame = NULL;
        ret = ctx->mpi->decode_get_frame(ctx->mpp, &frame);

        pthread_mutex_unlock(&dec->decoder_mutex);

        if (ret != MPP_OK || !frame) {
            if (ret != MPP_ERR_TIMEOUT)
                LOGE("failed to get frame\n");
            goto next;
        }

        pthread_mutex_lock(&ctx->ioctl_mutex);

        if (!dec->mpp_streaming)
            goto next_locked;

        /* Handle info change frame */
        if (mpp_frame_get_info_change(frame)) {
            rkmpp_apply_info_change(dec, frame);
            goto next_locked;
        }

        /* Handle eos frame, returning eos packet to userspace */
        if (mpp_frame_get_eos(frame)) {
            if (dec->eos_packet) {
                assert(ctx->output.streaming);

                dec->eos_packet->bytesused = 0;

                LOGV(1, "return eos packet: %d\n", dec->eos_packet->index);

                pthread_mutex_lock(&ctx->output.queue_mutex);
                TAILQ_INSERT_TAIL(&ctx->output.avail_buffers, dec->eos_packet, entry);
                rkmpp_buffer_set_available(dec->eos_packet);
                pthread_mutex_unlock(&ctx->output.queue_mutex);
                dec->eos_packet = NULL;
            }

            goto next_locked;
        }

        if (!ctx->capture.streaming)
            goto next_locked;

        /* Handle normal frame */
        buffer = mpp_frame_get_buffer(frame);
        if (!buffer) {
            LOGE("frame(%lld) doesn't have buf\n", mpp_frame_get_pts(frame));

            goto next_locked;
        }

        mpp_buffer_inc_ref(buffer);

        index = mpp_buffer_get_index(buffer);
        rkmpp_buffer = &ctx->capture.buffers[index];

        rkmpp_buffer->timestamp = mpp_frame_get_pts(frame);
        rkmpp_buffer_set_locked(rkmpp_buffer);

        if (mpp_frame_get_errinfo(frame) || mpp_frame_get_discard(frame)) {
            LOGE("frame err or discard\n");
            rkmpp_buffer->bytesused = 0;
            rkmpp_buffer_set_error(rkmpp_buffer);
        } else {
            /* Size of NV12 image */
            rkmpp_buffer->bytesused = dec->video_info.hor_stride * dec->video_info.ver_stride * 3 / 2;
        }

        LOGV(3, "return frame: %d(%" PRIu64 ")\n", index, rkmpp_buffer->timestamp);

        pthread_mutex_lock(&ctx->capture.queue_mutex);
        TAILQ_INSERT_TAIL(&ctx->capture.avail_buffers, rkmpp_buffer, entry);
        rkmpp_buffer_set_available(rkmpp_buffer);
        pthread_mutex_unlock(&ctx->capture.queue_mutex);
next_locked:
        pthread_mutex_unlock(&ctx->ioctl_mutex);
next:
        /* Update poll event after every loop */
        pthread_mutex_lock(&ctx->ioctl_mutex);
        rkmpp_update_poll_event(ctx);
        pthread_mutex_unlock(&ctx->ioctl_mutex);

        if (frame)
            mpp_frame_deinit(&frame);
    }

    LEAVE();
    return NULL;
}


static int codec_init(void* userdata) {
    struct cuse_codec* codec = userdata;
    struct rkmpp_dec_context *dec;
    MPP_RET ret;
    struct rkmpp_context *ctx = context_init();

    ENTER();

    if (!ctx)
        RETURN_ERR(ENOMEM, MPP_ERR_NOMEM);

    dec = (struct rkmpp_dec_context*) calloc(1, sizeof(struct rkmpp_dec_context));
    if (!dec)
        RETURN_ERR(ENOMEM, MPP_ERR_NOMEM);
    ctx->subctx = dec;
    codec->priv = ctx;
    dec->ctx = ctx;

    /* Using external buffer mode to limit buffers */
    ret = mpp_buffer_group_get_external(&ctx->capture.external_group, MPP_BUFFER_TYPE_DRM);
    if (ret != MPP_OK) {
        LOGE("failed to use mpp ext drm buf group\n");
        errno = ENODEV;
        free(dec);
        RETURN_ERR(errno, MPP_ERR_NOMEM);
    }

    ctx->formats = rkmpp_dec_fmts;
    ctx->num_formats = ARRAY_SIZE(rkmpp_dec_fmts);

    pthread_cond_init(&dec->decoder_cond, NULL);
    pthread_mutex_init(&dec->decoder_mutex, NULL);
    pthread_create(&dec->decoder_thread, NULL, decoder_thread_fn, dec);

    LEAVE();
    return MPP_OK;
}

static void codec_deinit(void *userdata) {
    struct cuse_codec* codec = userdata;
    struct rkmpp_context *ctx = codec->priv;
    struct rkmpp_dec_context *dec = NULL;

    if(!ctx || !ctx->subctx)
        return;

    dec = (struct rkmpp_dec_context *)ctx->subctx;

    ENTER();

    if (dec->decoder_thread) {
        pthread_cancel(dec->decoder_thread);
        pthread_join(dec->decoder_thread, NULL);
    }

    if (dec->mpp_streaming) {
        ctx->mpi->reset(ctx->mpp);
        mpp_destroy(ctx->mpp);
    }

    LEAVE();

    free(dec);
    ctx->subctx = NULL;

    context_destroy(ctx);
    codec->priv = NULL;
}

static struct cuse_ioctl ioctls[] = {
    { .cmd = (int)VIDIOC_QUERYCAP, .callback = rkmpp_ioctl_querycap }
};

static struct cuse_codec decoder = {
    .filename = "video0-mpp-dec",
    .init = codec_init,
    .deinit = codec_deinit,
    .ioctls = ioctls,
    .num_ioctls = ARRAY_SIZE(ioctls)
};

int main(int argc, char **argv) {
    return initcodec(&decoder, argc, argv);
}

