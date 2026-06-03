// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#define LOG_TAG "SmcInvoke_TZCom"
//#define LOG_NDEBUG 0	//comment out to disable ALOGV

#include <utils/Log.h>
#include <utils/Errors.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <chrono>

#include "SmcInvokeCredAPI.h"
#include "qcbor.h"
#include "object.h"
#include "MinkDescriptor.h"
#include "fdwrapper.h"
#include "CIO.h"
#include "IClientEnv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SMCINVOKE_CREDENTIALS_BUF_SIZE_INC 4096

using namespace android;

static void* getSelfCreds(size_t* encodedBufLen) {
    QCBOREncodeContext ECtx;
    uint8_t* credential_buf = nullptr;
    size_t buf_len = 0;
    int error = QCBOR_ERR_BUFFER_TOO_SMALL;
    EncodedCBOR Enc = {0};

    while(error == QCBOR_ERR_BUFFER_TOO_SMALL) {
        buf_len += SMCINVOKE_CREDENTIALS_BUF_SIZE_INC;
        free(credential_buf);
        credential_buf = (uint8_t*)calloc(1, buf_len);
        if (!credential_buf) {
            ALOGE("Could not allocate %zd memory for credentials\n", buf_len);
            return nullptr;
        }

        //Write UID and system time and create a cbor buf for TZ to hold onto.
        QCBOREncode_Init(&ECtx, credential_buf, buf_len);
        QCBOREncode_OpenMap(&ECtx);
        QCBOREncode_AddInt64ToMapN(&ECtx, AttrUid, getuid());
        auto time = std::chrono::system_clock::now().time_since_epoch();
        auto timems  = std::chrono::duration_cast<std::chrono::milliseconds>(time);
        QCBOREncode_AddInt64ToMapN(&ECtx, AttrSystemTime, (int64_t)timems.count());
        QCBOREncode_CloseMap(&ECtx);
        error = QCBOREncode_Finish2(&ECtx, &Enc);
    }

    *encodedBufLen = Enc.Bytes.len;
    return credential_buf;
}

/**@brief get root object
* This is used to create a root IClientEnv Obj when it starts up;
* it support default 16 callback threads and 4K callback request buffer.
*
* @ param[out] rootobj: root IClientEnv Obj
*
* return value:  Object_OK - success; Object_ERROR - failure
*/
int TZCom_getRootEnvObject (Object *obj)
{
	return MinkDescriptor_getRootEnv(DEFAULT_CBOBJ_THREAD_CNT,
					DEFAULT_CBREQ_BUFFER_SIZE, obj);
}

/**@brief get root object and create callback context
* This is used by client to create a root IClientEnv Obj and
* create callback context to support callback object
*
* @ param[in] cbthread_cnt:  cb thread count
* @ param[in] cbbuf_len:     cb request buffer length
* @ param[out] rootobj:      root Obj
*
* return value:  Object_OK - success; Object_ERROR - failure
*/
int TZCom_getRootEnvObjectWithCB (size_t cbthread_cnt, size_t cbbuf_len, Object *obj)
{
	return MinkDescriptor_getRootEnv(cbthread_cnt, cbbuf_len, obj);
}

int TZCom_getClientEnvObject (Object *clientEnvObj)
{
    Object rootObj = Object_NULL;

    int ret;
    if ((ret = TZCom_getRootEnvObject(&rootObj)) != 0) {
        ALOGE("TZCom_getRootEnvObject ret=%d\n", ret);
        return -1;
    }

    size_t len = 0;
    ALOGV("TZCom_getRootEnvObject calling getSelfCreds\n");
    void* creds = getSelfCreds(&len);
    if (!creds) {
        ALOGE("TZCom_getRootEnvObject creds=NULL\n");
        Object_release(rootObj);
        return -1;
    }
    Object credentials = Object_NULL;
    ALOGV("TZCom_getRootEnvObject calling CIO_open\n");
    CIO_open(creds, len, &credentials);

    ALOGV("TZCom_getRootEnvObject calling IClientEnv_registerAsClient\n");
    /* We need to use buffer based credential until Tunnel Invoke solves
     * problem of CBObj based credentials.
     */
    ret = IClientEnv_registerAsClient(rootObj, credentials, clientEnvObj);
    if (ret == Object_ERROR_INVALID || ret == Object_ERROR_BADOBJ) {
        ALOGD("Failed to register client, fall back to legacy register");
        ret = IClientEnv_registerLegacy(rootObj, creds, len, clientEnvObj);
        if (ret != 0) {
            ALOGE("IClientEnv_register ret=%d\n", ret);
        }
  }

    free(creds);
    Object_release(rootObj);
    Object_release(credentials);

    ALOGV("TZCom_getRootEnvObject return, ret=%d\n", ret);
    return ret;
}

/**@brief Client get a new IClientEnv obj
* Provides an IClientEnv obj that registered into TZ with client's credentials
* @ param[in] credentials: client's ICredentials obj
* @ param[out] clientEnvObj: client's IClientEnv object
*
* return value:  0- success; non-0 failure
*/
int TZCom_getClientEnvObjectWithCreds (Object *clientEnvObj, Object credentials)
{
    Object rootObj = Object_NULL;
    int ret;
    size_t len = 0;

    if (Object_isNull(credentials)) {
        ALOGE("credentials is a NULL obj\n");
        ret = Object_ERROR_BADOBJ;
        goto exit;
    }

    if ((ret = TZCom_getRootEnvObject(&rootObj)) != 0) {
        ALOGE("TZCom_getRootEnvObject ret=%d\n", ret);
        ret = Object_ERROR;
        goto exit;
    }

    ALOGV("TZCom_getRootEnvObject calling IClientEnv_registerAsClient\n");
    ret = IClientEnv_registerWithCredentials(rootObj, credentials, clientEnvObj);
    if (ret == Object_ERROR_INVALID || ret == Object_ERROR_BADOBJ) {
        ALOGD("Failed to register client, ret=%d\n", ret);
        goto exit;
    }

exit:
    if (!Object_isNull(rootObj)) {
        Object_release(rootObj);
    }

    ALOGV("return, ret=%d\n", ret);
    return ret;
}

/**@brief Client get a fd obj
*
* @ param[in] fd: the fd to be wrapped into an obj.
* @ param[out] obj: fd object that takes ownership of fd
*                   i.e.release of obj would close fd.
*
* return value:  0 - success; -1 - failure
*/
int TZCom_getFdObject (int fd, Object *obj)
{
    if (fd < 0 || obj == NULL) {
        ALOGE("invalid fd %d or NULL obj pointer\n", fd);
        return Object_ERROR;
    }

    *obj = FdWrapper_new(fd);
    if (Object_isNull(*obj))
        return Object_ERROR_KMEM;
    return Object_OK;
}

#ifdef __cplusplus
}
#endif
