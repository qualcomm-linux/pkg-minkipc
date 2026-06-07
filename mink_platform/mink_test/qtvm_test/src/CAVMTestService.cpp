// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "CAVMTestService.h"
#include <pthread.h>
#include <string.h>
#include "IAVMTestService.h"
#include "IAVMTestService_invoke.h"
#include "IModule.h"
#include "IModule_invoke.h"
#include "TUtils.h"
#include "VmOsal.h"
#include "heap.h"
#include "memscpy.h"
#include "object.h"

#define LOG(xx_fmt, ...)                                        \
    {                                                           \
        printf("HLOSMinkTestService : " xx_fmt, ##__VA_ARGS__); \
        fflush(stdout);                                         \
    }

typedef struct {
    int32_t refs;
    Object cred;
} TestService;

typedef struct {
    int32_t refs;
} TestIModule;

//----------------------------------------------------------------
// A simple service object, for testing
//----------------------------------------------------------------

static int32_t CAVMTestService_release(TestService *me)
{
    if (vm_osal_atomic_add(&me->refs, -1) == 0) {
        LOG("released TestService = %p\n", me);
        Object_ASSIGN_NULL(me->cred);
        HEAP_FREE_PTR(me);
    }
    return Object_OK;
}

static int32_t CAVMTestService_retain(TestService *me)
{
    if (NULL != me && vm_osal_atomic_add(&me->refs, 0) > 0) {
        vm_osal_atomic_add(&me->refs, 1);
        return Object_OK;
    }
    return Object_ERROR;
}

static int32_t CAVMTestService_QRTRSendBuff(TestService *me, const void *buff_ptr, size_t buff_len,
                                            void *out_ptr, size_t out_len, size_t *out_lenout)
{
    int32_t ret = Object_ERROR;
    const uint8_t *buff = (const uint8_t *)buff_ptr;
    uint8_t out[IAVMTestService_MAXSENDSTRINGLEN] = {0};

    if (buff_len > IAVMTestService_MAXSENDSTRINGLEN || out_len > IAVMTestService_MAXSENDSTRINGLEN ||
        buff_len > out_len) {
        LOG_ERR("Buff length is bigger than %lu\n", IAVMTestService_MAXSENDSTRINGLEN);
        ret = IAVMTestService_ERROR_INVALID_PARAMETER;
        goto exit;
    }

    *out_lenout = memscpy(out, out_len, buff, buff_len);
    T_CHECK(buff_len == *out_lenout);
    LOG("Recived Buff: %s\n", out);
    *out_lenout = memscpy(out_ptr, out_len, out, strlen((const char *)out));
    T_CHECK(strlen((const char *)out) == *out_lenout);

    ret = Object_OK;

exit:
    return ret;
}

static int32_t CAVMTestService_open(TestService *me)
{
    LOG("Connection success.\n");
    return Object_OK;
}

static IAVMTestService_DEFINE_INVOKE(CAVMTestService_invoke, CAVMTestService_, TestService *);

//----------------------------------------------------------------
// A simple module object, for testing
//----------------------------------------------------------------

static int32_t TestIModule_release(TestIModule *me)
{
    if (vm_osal_atomic_add(&me->refs, -1) == 0) {
        LOG("released TestModule = %p\n", me);
        HEAP_FREE_PTR(me);
    }
    return Object_OK;
}

static int32_t TestIModule_retain(TestIModule *me)
{
    if (NULL != me && vm_osal_atomic_add(&me->refs, 0) > 0) {
        vm_osal_atomic_add(&me->refs, 1);
        return Object_OK;
    }
    return Object_ERROR;
}

static int32_t TestIModule_shutdown(TestIModule *me)
{
    LOG("IModule_shutdown!\n");
    return Object_OK;
}

static int32_t TestIModule_open(TestIModule *me, uint32_t id, Object credentials, Object *objOut)
{
    if (id != CAVMCommonTestService_UID && id != CAVMStressTestService_UID) {
        return Object_ERROR;
    }
    LOG("TestIModule_open!\n");
    TestService *service = HEAP_ZALLOC_TYPE(TestService);
    if (!service) {
        return Object_ERROR_MEM;
    }

    service->refs = 1;
    Object_ASSIGN(service->cred, credentials);
    *objOut = (Object){CAVMTestService_invoke, service};

    return Object_OK;
}

static IModule_DEFINE_INVOKE(TestIModule_invoke, TestIModule_, TestIModule *);

int32_t CAVMTestIModule_new(Object *objOut)
{
    TestIModule *me = HEAP_ZALLOC_TYPE(TestIModule);
    if (!me) {
        return Object_ERROR_MEM;
    }

    me->refs = 1;
    *objOut = (Object){TestIModule_invoke, me};

    return Object_OK;
}
