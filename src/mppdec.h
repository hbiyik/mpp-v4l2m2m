/*
 * mppdec.h
 *
 *  Created on: Dec 26, 2023
 *      Author: boogie
 */

#ifndef SRC_MPPDEC_H_
#define SRC_MPPDEC_H_

#include "rkmpp.h"

#ifndef V4L2_PIX_FMT_VP9
#define V4L2_PIX_FMT_VP9    v4l2_fourcc('V', 'P', '9', '0') /* VP9 */
#endif

#ifndef V4L2_PIX_FMT_HEVC
#define V4L2_PIX_FMT_HEVC   v4l2_fourcc('H', 'E', 'V', 'C') /* HEVC */
#endif

#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1    v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif

/**
 * struct rkmpp_video_info - Video information
 * @valid:      Data is valid.
 * @dirty:      Data is dirty(have not applied to mpp).
 * @event:      Pending V4L2 src_change event.
 * @mpp_format:     MPP frame format.
 * @width:      Video width.
 * @height:     Video height.
 * @hor_stride:     Video horizontal stride.
 * @ver_stride:     Video vertical stride.
 * @size:       Required video buffer size.
 */
struct rkmpp_video_info {
    bool valid;
    bool dirty;
    bool event;

    MppFrameFormat mpp_format;
    uint32_t width;
    uint32_t height;
    uint32_t hor_stride;
    uint32_t ver_stride;

    uint32_t size;
};

/**
 * struct rkmpp_dec_context - Context private data for decoder
 * @ctx:        Common context data.
 * @video_info:     Video information.
 * @event_subscribed:   V4L2 event subscribed.
 * @mpp_streaming:  The mpp is streaming.
 * @decoder_thread: Handler of the decoder thread.
 * @decoder_cond:   Condition variable for streaming flag.
 * @decoder_mutex:  Mutex for streaming flag and buffers.
 */
struct rkmpp_dec_context {
    struct rkmpp_context *ctx;
    struct rkmpp_video_info video_info;

    bool event_subscribed;

    bool mpp_streaming;

    struct rkmpp_buffer *eos_packet;

    pthread_t decoder_thread;
    pthread_cond_t decoder_cond;
    pthread_mutex_t decoder_mutex;
};

#endif /* SRC_MPPDEC_H_ */
