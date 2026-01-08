// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "morelib/dev.h"
#include "morelib/devfs.h"
#include "morelib/fatfs.h"
#include "morelib/littlefs.h"
#include "morelib/loop.h"
#include "morelib/mem.h"
#include "morelib/mtdblk.h"
#include "morelib/tty.h"
#include "rp2/mtd.h"
#include "rp2/sdcard.h"
#include "rp2/term_uart.h"
#include "tinyuf2/tinyuf2.h"
#include "tinyusb/term_usb.h"


const struct dev_driver *dev_drvs[] = {
    &mem_drv,
    &tty_drv,
    &loop_drv,
    &mtd_drv,
    &mtdblk_drv,
    &sdcard_drv,
    &term_uart_drv,
    &term_usb_drv,
    &tinyuf2_drv,
};
const size_t dev_num_drvs = sizeof(dev_drvs) / sizeof(dev_drvs[0]);


const struct devfs_entry devfs_entries[] = {
    { "/", S_IFDIR, 0 },

    { "/mem", S_IFCHR, DEV_MEM },
    { "/null", S_IFCHR, DEV_NULL },
    { "/zero", S_IFCHR, DEV_ZERO },
    { "/full", S_IFCHR, DEV_FULL },

    { "/firmware", S_IFBLK, DEV_MTDBLK0 },
    { "/storage", S_IFBLK, DEV_MTDBLK1 },
    { "/psram", S_IFBLK, DEV_MTDBLK2 },

    { "/mtd0", S_IFCHR, DEV_MTD0 },
    { "/mtd1", S_IFCHR, DEV_MTD1 },
    { "/mtd2", S_IFCHR, DEV_MTD2 },
    { "/mtd3", S_IFCHR, DEV_MTD3 },

    { "/mtdblock0", S_IFBLK, DEV_MTDBLK0 },
    { "/mtdblock1", S_IFBLK, DEV_MTDBLK1 },
    { "/mtdblock2", S_IFBLK, DEV_MTDBLK2 },
    { "/mtdblock3", S_IFBLK, DEV_MTDBLK3 },

    { "/tty", S_IFCHR, DEV_TTY },
    { "/tmux", S_IFCHR, DEV_TMUX },

    { "/ttyS0", S_IFCHR, DEV_TTYS0 },
    { "/ttyS1", S_IFCHR, DEV_TTYS1 },

    { "/ttyUSB0", S_IFCHR, DEV_TTYUSB0 },

    { "/uf2", S_IFBLK, DEV_UF2 },

    { "/sdcard0", S_IFBLK, DEV_MMCBLK0 | 0x00 | 17 },
    { "/sdcard1", S_IFBLK, DEV_MMCBLK0 | 0x80 | 9 },

    { "/loop0", S_IFBLK, DEV_LOOP0 },
    { "/loop1", S_IFBLK, DEV_LOOP1 },
};

const size_t devfs_num_entries = sizeof(devfs_entries) / sizeof(devfs_entries[0]);


const struct vfs_filesystem *vfs_fss[] = {
    &devfs_fs,
    &fatfs_fs,
    &littlefs_fs,
};

const size_t vfs_num_fss = sizeof(vfs_fss) / sizeof(vfs_fss[0]);

#if MICROPY_PY_LWIP
#include "morelib/lwip/lwip.h"

const struct socket_family *socket_families[] = {
    &lwip_ipv4_af,
    &lwip_ipv6_af,
};
#else
const struct socket_family *socket_families[] = {};
#endif
const size_t socket_num_families = sizeof(socket_families) / sizeof(socket_families[0]);
