#define FUSE_USE_VERSION 31

#include <cuse_lowlevel.h>
#include <fuse_opt.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "cusedev.h"
#include "logger.h"
#include "utils.h"

int app_log_level = 0;

static const char *usage =
"usage: executable [options]\n"
"\n"
"options:\n"
"    --help|-h                  print this help message\n"
"    -d   -o debug              enable debug output (implies -f)\n"
"    --loglevel=LEVEL|-m level  device minor number\n"
"    -s                         disable multi-threaded operation\n"
"\n";

static void codec_open(fuse_req_t req, struct fuse_file_info *fi) {
    struct cuse_codec *codec = fuse_req_userdata(req);
    int ret = codec->init(codec);

    if (!ret)
        fuse_reply_open(req, fi);
    else
        fuse_reply_err(req, ret);
}

static void codec_close(fuse_req_t req, struct fuse_file_info *fi) {
    struct cuse_codec *codec = fuse_req_userdata(req);
    codec->deinit(codec);
    fuse_reply_err(req, 0);
}

static void codec_ioctl(fuse_req_t req, int cmd, void *arg, struct fuse_file_info *fi, unsigned flags,
        const void *in_buf, size_t in_bufsz, size_t out_bufsz) {
    struct cuse_codec *codec = fuse_req_userdata(req);
    int ret;
    bool iswrite = getbit(cmd, 30) && !in_bufsz;
    bool isread = getbit(cmd, 31) && !out_bufsz;
    int argsize = getint(cmd, 16, 14);
    const char* ioctlcmd;

    for (int i = 0; i < codec->num_ioctls; i++) {
        if (cmd == codec->ioctls[i].cmd) {
            // request the buffers
            struct iovec iovout = { arg, argsize };
            struct iovec iovin = { arg, argsize };
            struct iovec *iovinp = iswrite ? &iovout : NULL;
            struct iovec *iovoutp = isread ? &iovin : NULL;

            if (iovinp || iovoutp) {
                fuse_reply_ioctl_retry(req, iovinp, !!iovinp, iovoutp, !!iovoutp);
            } else {
                void* out_buf = iswrite && !isread ? NULL : calloc(1, argsize);
                ret = codec->ioctls->callback(codec, in_buf, out_buf);
                if (ret < 0)
                    fuse_reply_err(req, ret);
                else {
                    fuse_reply_ioctl(req, 0, out_buf, argsize);
                }
                if(out_buf)
                    free(out_buf);
            }
            return;
        }
    }

    ioctlcmd = rkmpp_cmd2str(cmd);
    if(strstr(ioctlcmd, "UNKNOWN"))
        LOGV(3, "Unsupported IOCTL: %d\n", cmd);
    else
        LOGE("Unsupported IOCTL: %s\n", ioctlcmd);
    fuse_reply_err(req, EINVAL);
}

static void codec_read(fuse_req_t req, size_t size, off_t off, struct fuse_file_info *fi) {
    (void) fi;

    fuse_reply_buf(req, "", size);
}

static void codec_write(fuse_req_t req, const char *buf, size_t size, off_t off, struct fuse_file_info *fi) {
    (void) fi;

    fuse_reply_write(req, size);
}

static void codec_poll(fuse_req_t req, struct fuse_file_info *fi, struct fuse_pollhandle *ph) {
    return;
}

struct params {
    int is_help;
    unsigned loglevel;

};

#define CUSE_OPT(t, p) { t, offsetof(struct params, p), 1 }

static const struct fuse_opt cuse_opts[] = {
    FUSE_OPT_KEY("-h",         0),
    FUSE_OPT_KEY("--help",     0),
    CUSE_OPT("--loglevel %d",  loglevel),
    CUSE_OPT("-l %d",         loglevel),
    FUSE_OPT_END
};

static int cuse_process_arg(void *data, const char *arg, int key,
                   struct fuse_args *outargs)
{
    struct params *param = data;

    (void)outargs;
    (void)arg;

    if(!key) {
        param->is_help = 1;
        fprintf(stderr, "%s", usage);
    }

    return 1;
}

static const struct cuse_lowlevel_ops cuse_clop = {
    .open       = codec_open,
    .release    = codec_close,
    .read       = codec_read,
    .write      = codec_write,
    .poll       = codec_poll,
    .ioctl      = codec_ioctl,
};

int initcodec(struct cuse_codec *codec, int argc, char **argv) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct params param = { 0 };
    char dev_name[128] = "DEVNAME=";
    const char *dev_info_argv[] = { dev_name };
    struct cuse_info ci;
    int ret = 1;

    strcat(dev_name, codec->filename);
    if (fuse_opt_parse(&args, &param, cuse_opts, cuse_process_arg)) {
        printf("failed to parse option\n");
        goto out;
    }

    if (param.is_help)
        goto out;

    app_log_level = param.loglevel;

    memset(&ci, 0, sizeof(ci));
    ci.dev_info_argc = 1;
    ci.dev_info_argv = dev_info_argv;
    ci.flags = CUSE_UNRESTRICTED_IOCTL;

    ret = cuse_lowlevel_main(args.argc, args.argv, &ci, &cuse_clop, codec);

    out:
    fuse_opt_free_args(&args);
    return ret;
}
