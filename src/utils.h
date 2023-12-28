/*
 * utils.h
 *
 *  Created on: Dec 26, 2023
 *      Author: boogie
 */

#ifndef SRC_UTILS_H_
#define SRC_UTILS_H_

#include "linux/videodev2.h"

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

/* From kernel's linux/kernel.h */
#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))

#define clamp(val, lo, hi)  min((typeof(val))max(val, lo), hi)

#define __round_mask(x, y)  ((__typeof__(x))((y)-1))
#define round_up(x, y)      ((((x)-1) | __round_mask(x, y))+1)

/* From kernel's linux/stddef.h */
#ifndef offsetof
#define offsetof(TYPE, MEMBER)  ((size_t)&((TYPE *)0)->MEMBER)
#endif

/* From kernel's v4l2-core/v4l2-ioctl.c */
#define CLEAR_AFTER_FIELD(p, field) \
    memset((uint8_t *)(p) + \
           offsetof(typeof(*(p)), field) + sizeof((p)->field), \
           0, sizeof(*(p)) - \
           offsetof(typeof(*(p)), field) - sizeof((p)->field))

#define getbit(var, bit)    (((int)var >> bit) & 1)
#define getint(var, start, len)    (((int)var >> start) & (unsigned)((2 << len) - 1))

static inline
const char* rkmpp_cmd2str(int cmd)
{
#define RKMPP_CMD2STR(cmd) case cmd: return #cmd

    switch (cmd) {
    RKMPP_CMD2STR(VIDIOC_QUERYCAP);
    RKMPP_CMD2STR(VIDIOC_ENUM_FMT);
    RKMPP_CMD2STR(VIDIOC_G_FMT);
    RKMPP_CMD2STR(VIDIOC_S_FMT);
    RKMPP_CMD2STR(VIDIOC_REQBUFS);
    RKMPP_CMD2STR(VIDIOC_QUERYBUF);
    RKMPP_CMD2STR(VIDIOC_G_FBUF);
    RKMPP_CMD2STR(VIDIOC_S_FBUF);
    RKMPP_CMD2STR(VIDIOC_OVERLAY);
    RKMPP_CMD2STR(VIDIOC_QBUF);
    RKMPP_CMD2STR(VIDIOC_EXPBUF);
    RKMPP_CMD2STR(VIDIOC_DQBUF);
    RKMPP_CMD2STR(VIDIOC_STREAMON);
    RKMPP_CMD2STR(VIDIOC_STREAMOFF);
    RKMPP_CMD2STR(VIDIOC_G_PARM);
    RKMPP_CMD2STR(VIDIOC_S_PARM);
    RKMPP_CMD2STR(VIDIOC_G_STD);
    RKMPP_CMD2STR(VIDIOC_S_STD);
    RKMPP_CMD2STR(VIDIOC_ENUMSTD);
    RKMPP_CMD2STR(VIDIOC_ENUMINPUT);
    RKMPP_CMD2STR(VIDIOC_G_CTRL);
    RKMPP_CMD2STR(VIDIOC_S_CTRL);
    RKMPP_CMD2STR(VIDIOC_G_TUNER);
    RKMPP_CMD2STR(VIDIOC_S_TUNER);
    RKMPP_CMD2STR(VIDIOC_G_AUDIO);
    RKMPP_CMD2STR(VIDIOC_S_AUDIO);
    RKMPP_CMD2STR(VIDIOC_QUERYCTRL);
    RKMPP_CMD2STR(VIDIOC_QUERYMENU);
    RKMPP_CMD2STR(VIDIOC_G_INPUT);
    RKMPP_CMD2STR(VIDIOC_S_INPUT);
    RKMPP_CMD2STR(VIDIOC_G_EDID);
    RKMPP_CMD2STR(VIDIOC_S_EDID);
    RKMPP_CMD2STR(VIDIOC_G_OUTPUT);
    RKMPP_CMD2STR(VIDIOC_S_OUTPUT);
    RKMPP_CMD2STR(VIDIOC_ENUMOUTPUT);
    RKMPP_CMD2STR(VIDIOC_G_AUDOUT);
    RKMPP_CMD2STR(VIDIOC_S_AUDOUT);
    RKMPP_CMD2STR(VIDIOC_G_MODULATOR);
    RKMPP_CMD2STR(VIDIOC_S_MODULATOR);
    RKMPP_CMD2STR(VIDIOC_G_FREQUENCY);
    RKMPP_CMD2STR(VIDIOC_S_FREQUENCY);
    RKMPP_CMD2STR(VIDIOC_CROPCAP);
    RKMPP_CMD2STR(VIDIOC_G_CROP);
    RKMPP_CMD2STR(VIDIOC_S_CROP);
    RKMPP_CMD2STR(VIDIOC_G_JPEGCOMP);
    RKMPP_CMD2STR(VIDIOC_S_JPEGCOMP);
    RKMPP_CMD2STR(VIDIOC_QUERYSTD);
    RKMPP_CMD2STR(VIDIOC_TRY_FMT);
    RKMPP_CMD2STR(VIDIOC_ENUMAUDIO);
    RKMPP_CMD2STR(VIDIOC_ENUMAUDOUT);
    RKMPP_CMD2STR(VIDIOC_G_PRIORITY);
    RKMPP_CMD2STR(VIDIOC_S_PRIORITY);
    RKMPP_CMD2STR(VIDIOC_G_SLICED_VBI_CAP);
    RKMPP_CMD2STR(VIDIOC_LOG_STATUS);
    RKMPP_CMD2STR(VIDIOC_G_EXT_CTRLS);
    RKMPP_CMD2STR(VIDIOC_S_EXT_CTRLS);
    RKMPP_CMD2STR(VIDIOC_TRY_EXT_CTRLS);
    RKMPP_CMD2STR(VIDIOC_ENUM_FRAMESIZES);
    RKMPP_CMD2STR(VIDIOC_ENUM_FRAMEINTERVALS);
    RKMPP_CMD2STR(VIDIOC_G_ENC_INDEX);
    RKMPP_CMD2STR(VIDIOC_ENCODER_CMD);
    RKMPP_CMD2STR(VIDIOC_TRY_ENCODER_CMD);
    RKMPP_CMD2STR(VIDIOC_DBG_S_REGISTER);
    RKMPP_CMD2STR(VIDIOC_DBG_G_REGISTER);
    RKMPP_CMD2STR(VIDIOC_S_HW_FREQ_SEEK);
    RKMPP_CMD2STR(VIDIOC_S_DV_TIMINGS);
    RKMPP_CMD2STR(VIDIOC_G_DV_TIMINGS);
    RKMPP_CMD2STR(VIDIOC_DQEVENT);
    RKMPP_CMD2STR(VIDIOC_SUBSCRIBE_EVENT);
    RKMPP_CMD2STR(VIDIOC_UNSUBSCRIBE_EVENT);
    RKMPP_CMD2STR(VIDIOC_CREATE_BUFS);
    RKMPP_CMD2STR(VIDIOC_PREPARE_BUF);
    RKMPP_CMD2STR(VIDIOC_G_SELECTION);
    RKMPP_CMD2STR(VIDIOC_S_SELECTION);
    RKMPP_CMD2STR(VIDIOC_DECODER_CMD);
    RKMPP_CMD2STR(VIDIOC_TRY_DECODER_CMD);
    RKMPP_CMD2STR(VIDIOC_ENUM_DV_TIMINGS);
    RKMPP_CMD2STR(VIDIOC_QUERY_DV_TIMINGS);
    RKMPP_CMD2STR(VIDIOC_DV_TIMINGS_CAP);
    RKMPP_CMD2STR(VIDIOC_ENUM_FREQ_BANDS);
    RKMPP_CMD2STR(VIDIOC_DBG_G_CHIP_INFO);
    RKMPP_CMD2STR(VIDIOC_QUERY_EXT_CTRL);
    default:
        return "UNKNOWN";
    }
}
#endif /* SRC_UTILS_H_ */