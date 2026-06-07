// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

/** This is the off-target version of "vmmem_wrapper.cpp"*/
/*  Implement some stubs to enable offtarget testing*/
#include <unistd.h>
#include <cstdint>
#include "vmmem_wrapper.h"

VmMem *CreateVmMem()
{
    return NULL;
}

void FreeVmMem(VmMem *instance)
{
}

VmHandle FindVmByName(VmMem *instance, char *cstr)
{
    return 1;
}

int ShareDmabufHandle(VmMem *instance, int dma_buf_fd, VmHandle *handles,
  uint32_t *perms, int nr, int64_t *memparcel_hdl)
{
    return 0;
}

int RetrieveDmabuf(VmMem *instance, VmHandle owner, VmHandle *handles,
  uint32_t *perms, int nr, int64_t memparcel_hdl)
{
    return 0;
}

int ReclaimDmabuf(VmMem *instance, int dma_buf_fd, int64_t memparcel_hdl)
{
    return 0;
}

int32_t LendDmabufHandle(VmMem *instance, int32_t dma_buf_fd, VmHandle *handles,
  uint32_t *perms, int32_t nr, int64_t *memparcel_hdl)
{
    return 0;
}
