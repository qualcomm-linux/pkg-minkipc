// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CONTROL_SOCKET_H
#define __CONTROL_SOCKET_H

#include <stdbool.h>
#include <stdint.h>

int32_t ControlSocket_enableSocketAttribute(const char *name);

int32_t ControlSocket_waitSocketEnable(int32_t fd, char *targetFileName);

int32_t ControlSocket_initSocketDirNotify(int32_t *notifyFd, int32_t *watchFd);

void ControlSocket_freeNotify(int32_t notifyFd, int32_t watchFd);

bool ControlSocket_checkSocketFile(const char *targetFile);

#endif  // __SOCKET_H
