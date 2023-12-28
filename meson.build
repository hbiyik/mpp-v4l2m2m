project('mpp-v4l2m2m', 'c')
src_common= ['src/cusedev.c', 'src/rkmpp.c']
src_dec = ['src/mppdec.c'] + src_common
deps = [dependency('fuse3'), dependency('rockchip_mpp')]
executable('mpp-v4l2m2m-dec', src_dec, dependencies : deps)

