struct cuse_ioctl {
    int cmd;
    int (*callback)(void *userdata, const void *in_buf, void *out_buf);
};

struct cuse_codec {
    char filename[64];
    int fd;
    int loglevel;
    void* priv;
    int (*init)(void *userdata);
    void (*deinit)(void *userdata);
    struct cuse_ioctl* ioctls;
    int num_ioctls;
};


int initcodec(struct cuse_codec* codec, int argc, char **argv);

