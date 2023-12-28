/*
 * rkmpp.h
 *
 *  Created on: Dec 26, 2023
 *      Author: boogie
 */
#ifndef SRC_RKMPP_H_
#define SRC_RKMPP_H_

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <rockchip/rk_mpi.h>
#include "linux/videodev2.h"

#include "logger.h"
#include "utils.h"


#define RKMPP_MB_DIM        16
#define RKMPP_SB_DIM        64

#define RKMPP_MAX_PLANE     3

#define RKMPP_MEM_OFFSET(type, index) \
    ((int64_t) ((type) << 16 | (index)))
#define RKMPP_MEM_OFFSET_TYPE(offset)   (int)((offset) >> 16)
#define RKMPP_MEM_OFFSET_INDEX(offset)  (int)((offset) & ((1 << 16) - 1))

#define RKMPP_HAS_FORMAT(ctx, format) \
    (!((format)->type != MPP_VIDEO_CodingUnused && (ctx)->codecs && \
       !strstr((ctx)->codecs, (format)->name)))

/**
 * struct rkmpp_fmt - Information about mpp supported format
 * @name:       Format's name.
 * @fourcc:     Format's forcc.
 * @num_planes: Number of planes.
 * @type:       Format's mpp coding type.
 * @format:     Format's mpp frame format.
 * @depth:      Format's pixel depth.
 * @frmsize:    V4L2 frmsize_stepwise.
 */
struct rkmpp_fmt {
    char *name;
    uint32_t fourcc;
    int num_planes;
    MppCodingType type;
    MppFrameFormat format;
    uint8_t depth[VIDEO_MAX_PLANES];
    struct v4l2_frmsize_stepwise frmsize;
};

/**
 * enum rkmpp_buffer_flag - Flags of rkmpp buffer
 * @ERROR:      Something wrong in the buffer.
 * @LOCKED:     Buffer been locked from mpp buffer group.
 * @EXPORTED:       Buffer been exported to userspace.
 * @QUEUED:     Buffer been queued.
 * @PENDING:        Buffer is in pending queue.
 * @AVAILABLE:      Buffer is in available queue.
 */
enum rkmpp_buffer_flag {
    RKMPP_BUFFER_ERROR  = 1 << 0,
    RKMPP_BUFFER_LOCKED = 1 << 1,
    RKMPP_BUFFER_EXPORTED   = 1 << 2,
    RKMPP_BUFFER_QUEUED = 1 << 3,
    RKMPP_BUFFER_PENDING    = 1 << 4,
    RKMPP_BUFFER_AVAILABLE  = 1 << 5,
    RKMPP_BUFFER_KEYFRAME   = 1 << 6,
};

/**
 * struct rkmpp_buffer - Information about mpp buffer
 * @entry:      Queue entry.
 * @rkmpp_buf:  Handle of mpp buffer.
 * @index:      Buffer's index.
 * @fd:         Buffer's dma fd.
 * @timestamp:  Buffer's timestamp.
 * @bytesused:  Number of bytes occupied by data in the buffer.
 * @length:     Buffer's length(planes).
 * @size:       Buffer's size.
 * @flags:      Buffer's flags.
 * @planes:     Buffer's planes info.
 */
struct rkmpp_buffer {
    TAILQ_ENTRY(rkmpp_buffer) entry;
    MppBuffer rkmpp_buf;

    int index;

    int fd;
    uint64_t timestamp;
    uint32_t bytesused;
    uint32_t length;
    uint32_t flags;
    uint32_t size;
    uint32_t type;

    struct {
        unsigned long userptr;
        int fd;
        uint32_t data_offset;
        uint32_t bytesused;
        uint32_t plane_size; /* bytesused - data_offset */
        uint32_t length;
    } planes[RKMPP_MAX_PLANE];
};

TAILQ_HEAD(rkmpp_buf_head, rkmpp_buffer);

/**
 * struct rkmpp_buf_head - Information about mpp buffer queue
 * @memory:         V4L2 memory type.
 * @streaming:      The queue is streaming.
 * @internal_group: Handle of mpp internal buffer group.
 * @external_group: Handle of mpp external buffer group.
 * @buffers:        List of buffers.
 * @num_buffers:    Number of buffers.
 * @avail_buffers:  Buffers ready to be dequeued.
 * @pending_buffers:Pending buffers for mpp.
 * @queue_mutex:    Mutex for buffer lists.
 * @rkmpp_format:   Mpp format.
 * @format:     V4L2 multi-plane format.
 */
struct rkmpp_buf_queue {
    enum v4l2_memory memory;

    bool streaming;

    MppBufferGroup internal_group;
    MppBufferGroup external_group;
    struct rkmpp_buffer *buffers;
    uint32_t num_buffers;

    struct rkmpp_buf_head avail_buffers;
    struct rkmpp_buf_head pending_buffers;
    pthread_mutex_t queue_mutex;

    const struct rkmpp_fmt *rkmpp_format;
    struct v4l2_pix_format_mplane format;
};

/**
 * struct rkmpp_context - Context data
 * @formats:        Supported formats.
 * @num_formats:    Number of formats.
 * @is_decoder:     Is decoder mode.
 * @nonblock:       Nonblock mode.
 * @eventfd:        File descriptor of eventfd.
 * @avail_buffers:  Buffers ready to be dequeued.
 * @pending_buffers:Pending buffers for mpp.
 * @mpp:            Handler of mpp context.
 * @mpi:            Handler of mpp api.
 * @output:         Output queue.
 * @capture:        Capture queue.
 * @ioctl_mutex:    Mutex.
 * @frames:         Number of frames reported.
 * @last_fps_time:  The last time to count fps.
 * @data:           Private data.
 */
struct rkmpp_context {
    struct rkmpp_fmt *formats;
    uint32_t num_formats;

    bool is_decoder;
    bool nonblock;
    int event_fd;
    int eventin_fd;
    int eventout_fd;

    MppCtx mpp;
    MppApi *mpi;

    struct rkmpp_buf_queue output;
    struct rkmpp_buf_queue capture;

    pthread_mutex_t ioctl_mutex;

    uint64_t frames;
    uint64_t last_fps_time;

    unsigned int max_width;
    unsigned int max_height;
    char *codecs;

    void *subctx;
};

#define RKMPP_BUFFER_FLAG_HELPER_GET(flag, name) \
static inline bool rkmpp_buffer_## name(struct rkmpp_buffer *buffer) \
{ \
    return !!(buffer->flags & flag); \
}

#define RKMPP_BUFFER_FLAG_HELPER_SET(flag, name) \
static inline void rkmpp_buffer_set_ ## name(struct rkmpp_buffer *buffer) \
{ \
    LOGV(4, "buffer: %d type: %d\n", buffer->index, buffer->type); \
    if (rkmpp_buffer_ ## name(buffer)) \
        LOGE("buffer: %d type: %d is already " #name "\n", \
             buffer->index, buffer->type); \
    buffer->flags |= flag; \
}

#define RKMPP_BUFFER_FLAG_HELPER_CLR(flag, name) \
static inline void rkmpp_buffer_clr_ ## name(struct rkmpp_buffer *buffer) \
{ \
    LOGV(4, "buffer: %d type: %d\n", buffer->index, buffer->type); \
    if (!rkmpp_buffer_ ## name(buffer)) \
        LOGE("buffer: %d type: %d is not " #name "\n", \
             buffer->index, buffer->type); \
    buffer->flags &= ~flag; \
}

#define RKMPP_BUFFER_FLAG_HELPERS(flag, name) \
    RKMPP_BUFFER_FLAG_HELPER_GET(flag, name) \
    RKMPP_BUFFER_FLAG_HELPER_SET(flag, name) \
    RKMPP_BUFFER_FLAG_HELPER_CLR(flag, name)

RKMPP_BUFFER_FLAG_HELPERS(RKMPP_BUFFER_ERROR, error)
RKMPP_BUFFER_FLAG_HELPERS(RKMPP_BUFFER_LOCKED, locked)
RKMPP_BUFFER_FLAG_HELPERS(RKMPP_BUFFER_EXPORTED, exported)
RKMPP_BUFFER_FLAG_HELPERS(RKMPP_BUFFER_QUEUED, queued)
RKMPP_BUFFER_FLAG_HELPERS(RKMPP_BUFFER_PENDING, pending)
RKMPP_BUFFER_FLAG_HELPERS(RKMPP_BUFFER_AVAILABLE, available)
RKMPP_BUFFER_FLAG_HELPERS(RKMPP_BUFFER_KEYFRAME, keyframe)

struct rkmpp_context *context_init();
void context_destroy(struct rkmpp_context *ctx);
int rkmpp_update_poll_event(struct rkmpp_context *ctx);
struct rkmpp_buf_queue* rkmpp_get_queue(struct rkmpp_context *ctx, enum v4l2_buf_type type);
int rkmpp_ioctl_querycap(void *userdata, const void* in_buf, void *out_buf);

#endif /* SRC_RKMPP_H_ */
