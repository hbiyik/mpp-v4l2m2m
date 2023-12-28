#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <linux/version.h>

#include "logger.h"
#include "rkmpp.h"
#include "cusedev.h"

int rkmpp_update_poll_event(struct rkmpp_context *ctx) {
    return 0;
}


static void rkmpp_destroy_buffers(struct rkmpp_buf_queue *queue) {
    unsigned int i;

    if (!queue->num_buffers)
        return;

    if (queue->buffers) {
        for (i = 0; i < queue->num_buffers; i++) {
            if (rkmpp_buffer_locked(&queue->buffers[i]))
                mpp_buffer_put(queue->buffers[i].rkmpp_buf);
        }

        free(queue->buffers);
        queue->buffers = NULL;
    }

    mpp_buffer_group_clear(queue->internal_group);

    if (queue->external_group)
        mpp_buffer_group_clear(queue->external_group);

    queue->num_buffers = 0;
}

struct rkmpp_buf_queue* rkmpp_get_queue(struct rkmpp_context *ctx, enum v4l2_buf_type type) {
    LOGV(4, "type = %d\n", type);

    switch (type) {
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        return &ctx->capture;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
        return &ctx->output;
    default:
        LOGE("invalid buf type\n");
        RETURN_ERR(EINVAL, NULL)
        ;
    }
}

int rkmpp_ioctl_querycap(void *userdata, const void* in_buf, void *out_buf) {
    struct cuse_codec *codec = userdata;
    struct rkmpp_context *ctx = codec->priv;
    struct v4l2_capability *cap = out_buf;

    ENTER();

    strncpy((char*) cap->driver, "rkmpp", sizeof(cap->driver));
    strncpy((char*) cap->card, "rkmpp", sizeof(cap->card));
    strncpy((char*) cap->bus_info, "platform: rkmpp", sizeof(cap->bus_info));

    cap->version = LINUX_VERSION_CODE;

    /* This is only a mem-to-mem video device. */
    cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
    cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

    cap->capabilities |= V4L2_CAP_EXT_PIX_FORMAT;
    cap->device_caps |= V4L2_CAP_EXT_PIX_FORMAT;

    LEAVE();
    return 0;
}

struct rkmpp_context* context_init() {
    struct rkmpp_context *ctx = NULL;
    MPP_RET ret;

    ENTER();

    ctx = (struct rkmpp_context*) calloc(1, sizeof(struct rkmpp_context));
    if (!ctx)
        RETURN_ERR(ENOMEM, NULL);

    pthread_mutex_init(&ctx->ioctl_mutex, NULL);
    pthread_mutex_init(&ctx->output.queue_mutex, NULL);
    pthread_mutex_init(&ctx->capture.queue_mutex, NULL);

    ret = mpp_buffer_group_get_internal(&ctx->output.internal_group, MPP_BUFFER_TYPE_DRM);
    if (ret != MPP_OK) {
        LOGE("failed to use mpp drm buf group\n");
        errno = ENODEV;
        goto err_free_ctx;
    }

    ret = mpp_buffer_group_get_internal(&ctx->capture.internal_group, MPP_BUFFER_TYPE_DRM);
    if (ret != MPP_OK) {
        LOGE("failed to use mpp drm buf group\n");
        errno = ENODEV;
        goto err_put_group;
    }

    LOGV(1, "ctx(%p)): inited,\n", (void* )ctx);

    LEAVE();
    return ctx;
err_put_group:
    if (ctx->output.internal_group)
        mpp_buffer_group_put(ctx->output.internal_group);

    if (ctx->capture.internal_group)
        mpp_buffer_group_put(ctx->capture.internal_group);
err_free_ctx:
    free(ctx);
    RETURN_ERR(errno, NULL);
}

void context_destroy(struct rkmpp_context *ctx) {
    ENTER();

    LOGV(1, "ctx(%p): closing\n", (void* )ctx);

    rkmpp_destroy_buffers(&ctx->output);

    if (ctx->output.internal_group)
        mpp_buffer_group_put(ctx->output.internal_group);

    if (ctx->output.external_group)
        mpp_buffer_group_put(ctx->output.external_group);

    rkmpp_destroy_buffers(&ctx->capture);

    if (ctx->capture.external_group)
        mpp_buffer_group_put(ctx->capture.external_group);

    if (ctx->capture.internal_group)
        mpp_buffer_group_put(ctx->capture.internal_group);

    if (ctx->codecs)
        free(ctx->codecs);

    LEAVE();

    free(ctx);
}
