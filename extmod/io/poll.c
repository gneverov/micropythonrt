// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <unistd.h>

#include "extmod/io/poll.h"
#include "extmod/modos_newlib.h"
#include "py/runtime.h"


static const struct vfs_file_vtable mp_poll_vtable = {
    .pollable = 1,
};

void mp_poll_init(mp_poll_t *self) {
    self->file = NULL;
    self->fd = -1;
}

int mp_poll_alloc(mp_poll_t *self, uint events) {
    assert(!self->file);
    self->file = calloc(1, sizeof(struct poll_file));
    if (!self->file) {
        return -1;
    }
    poll_file_init(self->file, &mp_poll_vtable, 0, events);
    return 0;
}

void mp_poll_deinit(mp_poll_t *self) {
    if (self->fd >= 0) {
        close(self->fd);
        self->fd = -1;
    }
    if (self->file) {
        poll_file_notify(self->file, 0, POLLERR);
        poll_file_release(self->file);
        self->file = NULL;
    }
}

bool mp_poll_wait(mp_poll_t *self, uint events, TickType_t *pxTicksToWait) {
    int ret;
    MP_OS_CALL(ret, poll_file_wait, self->file, events, pxTicksToWait);
    if (ret >= 0) {
        return true;
    }
    else if (errno == EAGAIN) {
        return false;
    }
    else {
        mp_raise_OSError(errno);
    }
}

mp_poll_t *mp_poll_get(mp_obj_t self_in) {
    mp_poll_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->file) {
        mp_raise_ValueError(MP_ERROR_TEXT("object closed"));
    }
    return self;
}

int mp_poll_fileno(mp_poll_t *self) {
    if (self->fd < 0) {
        self->fd = poll_file_fd(self->file);
    }
    return self->fd;
}
