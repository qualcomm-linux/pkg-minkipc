// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _VMMEM_WRAPPER_H_
#define _VMMEM_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VmMem VmMem;
typedef int32_t VmHandle;

#define VMMEM_READ (1U << 0)
#define VMMEM_WRITE (1U << 1)
#define VMMEM_EXEC (1U << 2)

VmMem *CreateVmMem();

void FreeVmMem(VmMem *instance);

int32_t IsExclusiveOwnerDmabuf(int32_t fd, bool *is_exclusive_owner);

VmHandle FindVmByName(VmMem *instance, char *cstr);

int32_t LendDmabufHandle(VmMem *instance, int32_t dma_buf_fd, VmHandle *handles, uint32_t *perms,
                         int32_t nr, int64_t *memparcel_hdl);

int32_t LendDmabuf(VmMem *instance, int32_t dma_buf_fd, VmHandle *handles, uint32_t *perms,
                   int32_t nr);

int32_t ShareDmabufHandle(VmMem *instance, int32_t dma_buf_fd, VmHandle *handles, uint32_t *perms,
                          int32_t nr, int64_t *memparcel_hdl);

int32_t ShareDmabuf(VmMem *instance, int32_t dma_buf_fd, VmHandle *handles, uint32_t *perms,
                    int32_t nr);

int32_t RetrieveDmabuf(VmMem *instance, VmHandle owner, VmHandle *handles, uint32_t *perms,
                       int32_t nr, int64_t memparcel_hdl);

int32_t ReclaimDmabuf(VmMem *instance, int32_t dma_buf_fd, int64_t memparcel_hdl);

int32_t RemoteAllocDmabuf(VmMem *instance, uint64_t size, VmHandle *handles, uint32_t *perms,
                          int32_t nr, char *c_src_dma_heap_name, char *c_dst_dma_heap_name);

#ifdef __cplusplus
} /* end extern "C" */
#endif

#endif /* _VMMEM_WRAPPER_H_ */
