/*
 * logger.h
 *
 *  Created on: Dec 26, 2023
 *      Author: boogie
 */

#ifndef SRC_LOGGER_H_
#define SRC_LOGGER_H_

#include <stdio.h>
#include <syscall.h>
#include <sys/time.h>
#include <unistd.h>

#define APP_VERSION "1.6.0~20231215"

extern int app_log_level;

#define gettid() syscall(SYS_gettid)

#define LOG(fmt, ...) do { \
    struct timeval tv; \
    gettimeofday(&tv, NULL); \
    printf("[%03ld.%03ld] [RKMPP] [%ld] %s(%d): " fmt, \
           tv.tv_sec % 1000, tv.tv_usec / 1000, gettid(), \
           __func__, __LINE__, ##__VA_ARGS__); \
    fflush(stdout); \
    } while (0)

#define LOGV(level, fmt, ...) \
    do { if (app_log_level >= level) LOG(fmt, ##__VA_ARGS__); } while (0)

#define LOGE(fmt, ...) LOG("ERR: " fmt, ##__VA_ARGS__)

#define RETURN_ERR(err, ret) \
    { errno = err; LOGV(2, "errno: %d\n", errno); return ret; }

#define ENTER()         LOGV(5, "ctx(%p): ENTER\n", (void *)ctx)
#define LEAVE()         LOGV(5, "ctx(%p): LEAVE\n", (void *)ctx)

#endif /* SRC_LOGGER_H_ */
