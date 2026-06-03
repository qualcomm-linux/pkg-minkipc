// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <stdio.h>
#include "ITMemLock.h"
#include "ITMemLock_invoke.h"
#include "MSMem.h"
#include "TMemLock.h"
#include "TUtils.h"
#include "heap.h"

#ifndef STUB
#include <fcntl.h>
#include <linux/qti-smmu-proxy.h>
#include <sys/ioctl.h>
#endif

typedef struct {
    int32_t refs;
    Object memObj;
} TMemLock;

static int32_t CTMemLock_retain(TMemLock *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t CTMemLock_release(TMemLock *me)
{
    int32_t ret = Object_OK;
    int32_t memFd = -1;
#ifndef STUB
    int32_t smmuFD = -1;
    struct smmu_proxy_acl_ctl actCtl;
#endif

    if (atomicAdd(&me->refs, -1) == 0) {
        T_CHECK_ERR(isMSMem(me->memObj, &memFd), ITMemLock_ERROR_BAD_MEMOBJ);

#ifndef STUB
        actCtl.dma_buf_fd = memFd;
        smmuFD = open("/dev/qti-smmu-proxy", O_RDWR);
        T_CHECK_ERR(smmuFD > 0, ITMemLock_ERROR_OPEN_DRIVER);

        ret = ioctl(smmuFD, QTI_SMMU_PROXY_AC_UNLOCK_BUFFER, &actCtl);
        T_CHECK_ERR(0 == ret, ITMemLock_ERROR_UNLOCK_BUFFER);
#endif

        Object_RELEASE_IF(me->memObj);
        HEAP_FREE_PTR(me);
    }

exit:
#ifndef STUB
    if (smmuFD >= 0) {
        close(smmuFD);
    }
#endif

    return ret;
}

static ITMemLock_DEFINE_INVOKE(CTMemLock_invoke, CTMemLock_, TMemLock *);

int32_t TMemLock_new(Object memObj, uint64_t uid, Object *objOut)
{
    int32_t ret = Object_OK;
    int32_t memFd = -1;
    TMemLock *me = NULL;

#ifndef STUB
    int32_t smmuFD = -1;
    struct smmu_proxy_acl_ctl actCtl;
#endif

    T_CHECK_ERR(isMSMem(memObj, &memFd), ITMemLock_ERROR_BAD_MEMOBJ);

#ifndef STUB
    actCtl.dma_buf_fd = memFd;
    smmuFD = open("/dev/qti-smmu-proxy", O_RDWR);
    T_CHECK_ERR(smmuFD > 0, ITMemLock_ERROR_OPEN_DRIVER);

    ret = ioctl(smmuFD, QTI_SMMU_PROXY_AC_LOCK_BUFFER, &actCtl);
    T_CHECK_ERR(0 == ret, ITMemLock_ERROR_LOCK_BUFFER);
#endif

    me = HEAP_ZALLOC_TYPE(TMemLock);
    T_CHECK_ERR(NULL != me, Object_ERROR_MEM);

    me->refs = 1;
    Object_INIT(me->memObj, memObj);
    *objOut = (Object){CTMemLock_invoke, me};

exit:
#ifndef STUB
    if (smmuFD >= 0) {
        close(smmuFD);
    }
#endif

    return ret;
}
