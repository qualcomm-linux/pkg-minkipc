// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "CTPowerService_open.h"
#include "CTPreLauncher.h"
#include "CTPreLauncher_open.h"
#include "CTProcessLoader.h"
#include "CTRegisterModule_priv.h"
#include "ErrorMap.h"
#include "IContainerModule.h"
#include "ICredentials.h"
#include "ITPowerService.h"
#include "ITPreLauncher.h"
#include "ITProcess.h"
#include "ITProcessLoader_invoke.h"
#include "ITRegisterModule.h"
#include "MemSCpy.h"
#include "MetadataInfo.h"
#include "MinkHub.h"
#include "PlatformConfig.h"
#include "TModule.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"
#include "libcontainer.h"
#include "object.h"
#include "osIndCredentials.h"

/**
 * QTVM Dynamic Process loader interface
 *
 * TProcessLoader is a Mink interface providing process-loading functions for
 * external clients (HLOS / TPs).
 * */

typedef struct {
    int32_t refs;
    cid_t cid;
    Object credentials;
} TProcessLoader;

static pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;

static Object gPreLauncherObj = Object_NULL;
static MinkHub *gMinkHub = NULL;

/**
 * Error conversion from ITPreLauncher to ITProcessLoader
 * */
/* clang-format off */
static const ErrorPair iTPreLauncherToITProcessLoaderErrors[] = {
    {ITPreLauncher_ERROR_INVALID_BUFFER,         ITProcessLoader_ERROR_INVALID_BUFFER},
    {ITPreLauncher_ERROR_GET_AUTH_SVC,           ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_ELF_SIGNATURE_ERROR,    ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_GET_ARB_VERSION,        ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_GET_RPMB_SVC,           ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_RPMB_FAILURE,           ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_ROLLBACK_FAILURE,       ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_METADATA_INVALID,       ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_NO_NAME_IN_METADATA,    ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_HEAP_MALLOC_FAILURE,    ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_PROC_NOT_LOADED,        ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_PROC_ALREADY_LOADED,    ITProcessLoader_ERROR_PROC_ALREADY_LOADED},
    {ITPreLauncher_ERROR_PROC_BLACKLISTED,       ITProcessLoader_ERROR_PROC_NOT_LOADED},
    {ITPreLauncher_ERROR_EMBED_MISSING_PROPERTY, ITProcessLoader_ERROR_EMBED_MISSING_PROPERTY},
};
/* clang-format on */

static const ErrorMap iTPreLauncherToITProcessLoaderErrorMap = {
    .errors = iTPreLauncherToITProcessLoaderErrors,
    .length = C_LENGTHOF(iTPreLauncherToITProcessLoaderErrors),
    .genericError = Object_ERROR,
    .startConversionAt = Object_ERROR_USERBASE};

static int32_t ITPreLauncherToITProcessLoaderError(int32_t error)
{
    return ErrorMap_convert(&iTPreLauncherToITProcessLoaderErrorMap, error);
}

/**
 * Error conversion from ITRegisterModule to ITProcessLoader
 * */
/* clang-format off */
static const ErrorPair iTRegisterModuleToITProcessLoaderErrors[] = {
    {ITRegisterModule_ERROR_TIMEDOUT,    ITProcessLoader_ERROR_IMODULE_REGISTRATION},
};

static const ErrorMap iTRegisterModuleToITProcessLoaderErrorMap = {
    .errors = iTRegisterModuleToITProcessLoaderErrors,
    .length = C_LENGTHOF(iTRegisterModuleToITProcessLoaderErrors),
    .genericError = Object_ERROR,
    .startConversionAt = Object_ERROR_USERBASE
};
/* clang-format on */

static int32_t ITRegisterModuleToITProcessLoaderError(int32_t error)
{
    return ErrorMap_convert(&iTRegisterModuleToITProcessLoaderErrorMap, error);
}

void CTProcessLoader_setMinkHub(MinkHub *hub)
{
    gMinkHub = hub;
}

static int32_t CTProcessLoader_registerClient(uint32_t pid, const char *mdStr,
                                              ITPreLauncher_authData *authData, Object iModule,
                                              Object tProcObj, cid_t cid)
{
    int32_t ret = Object_OK;
    Object tMod = Object_NULL;
    Object processCred = Object_NULL;
    MetadataInfo *metaData = NULL;
    const uint32_t *services = NULL;
    uint32_t serviceLen = 0;
    const uint32_t *privileges = NULL;
    uint32_t privilegeLen = 0;
    SHA256Hash hashValue = {{0}};
    uint32_t uid = 0;
    const char *appName = NULL;

    T_GUARD(MetadataInfo_new(&metaData, mdStr, strlen(mdStr)));
    T_GUARD(MetadataInfo_getServices(metaData, &services, &serviceLen));
    T_CHECK_ERR(serviceLen > 0, Object_ERROR_INVALID);

    T_GUARD(MetadataInfo_getPrivileges(metaData, &privileges, &privilegeLen));
    appName = MetadataInfo_getName(metaData);

    // The QTVM TA is running in a container, and libcontainer hardcode the uid as following rules:
    // 1000 for system and embedded TA, 10000 for downloadable TA.
    if (cid == DL_OEM_SIGNED) {
        uid = 10000;
    } else {
        uid = 1000;
    }

    memscpy(hashValue.val, sizeof(hashValue.val), authData->hash, sizeof(authData->hash));

    T_CALL(OSIndCredentials_newProcessCred(0, appName, &hashValue, 0, 0, NULL, 0,
                                           (const char *)authData->domain, pid, uid, &processCred));

    T_GUARD(TModule_registerTModule(gMinkHub, pid, services, serviceLen, privileges, privilegeLen,
                                    processCred, iModule, &tMod));

    T_GUARD(ITProcess_setTModule(tProcObj, tMod));
    T_GUARD(IContainerModule_enable(iModule));

exit:
    Object_ASSIGN_NULL(tMod);
    Object_ASSIGN_NULL(processCred);
    MetadataInfo_destruct(metaData);
    return ret;
}

/**
 * Launch a process, add a TModule for the new process, acquire the IModule from
 * that new process, attach it to the associated TModule, and attach the TModule
 * to the TProcess object.
 *
 * In:     memObj: Memory Object of the buffer which contains the ELF
 * Out:    tProcCtrlObj: TProcessController of the new process
 *
 * Return: Object_OK if launched successfully
 *         ITProcessLoader_ERROR_PROC_ALREADY_LOADED if process already running
 *         all else if failure
 */
static int32_t _load(Object memObj, cid_t cid, Object *tProcCtrlObj)
{
    int32_t ret = Object_ERROR;
    Object tProcObj = Object_NULL;
    Object iModule = Object_NULL;
    uint32_t pid = 0;
    uint8_t mdStr[MAX_MDSTR_LEN] = {0};
    size_t mdStrLenOut = 0;
    size_t mdStrLen = MAX_MDSTR_LEN;
    ITPreLauncher_authData mdStruct = {0};
    Object wakeLockObj = Object_NULL;
    Object powerServiceObj = Object_NULL;

    pthread_mutex_lock(&gLock);

    T_CHECK_ERR(!Object_isNull(gPreLauncherObj), ITProcessLoader_ERROR_LOADER_NOT_READY);

    // Ensure VM doesn't go to sleep during call
    T_GUARD(CTPowerServiceFactory_open(0, Object_NULL, &powerServiceObj));
    T_CALL(ITPowerService_acquireWakeLock(powerServiceObj, &wakeLockObj));

    T_CALL_REMAP(ITPreLauncher_launch(gPreLauncherObj, memObj, cid, &pid, mdStr, mdStrLen,
                                      &mdStrLenOut, &mdStruct, tProcCtrlObj, &tProcObj),
                 ITPreLauncherToITProcessLoaderError);

    T_CHECK((0 < mdStrLenOut) && (mdStrLenOut <= mdStrLen));
    T_CHECK('\0' == mdStr[mdStrLenOut - 1]);
    T_CHECK('\0' == mdStruct.domain[C_LENGTHOF(mdStruct.domain) - 1]);

    T_CALL_REMAP(CTRegisterModule_getIModuleFromPendingList(pid, &iModule),
                 ITRegisterModuleToITProcessLoaderError);
    T_CALL(CTProcessLoader_registerClient(pid, (const char *)mdStr, &mdStruct, iModule, tProcObj,
                                          cid));

exit:
    pthread_mutex_unlock(&gLock);
    if (ret && (ret != ITProcessLoader_ERROR_PROC_ALREADY_LOADED)) {
        Object_ASSIGN_NULL(*tProcCtrlObj);
        // Make sure process is killed if it was launched
        if (!Object_isNull(tProcObj)) {
            ITProcess_forceClose(tProcObj);
        }
    }
    /* TProcess object is no longer needed. Release so that refcount accurately
     * reflects current references. */
    Object_ASSIGN_NULL(tProcObj);
    Object_ASSIGN_NULL(iModule);
    Object_ASSIGN_NULL(wakeLockObj);
    Object_ASSIGN_NULL(powerServiceObj);

    return ret;
}

static int32_t CTProcessLoader_loadFromBuffer(TProcessLoader *me, Object memObj,
                                              Object *tProcCtrlObj)
{
    int32_t ret = Object_ERROR;

    T_CALL(_load(memObj, me->cid, tProcCtrlObj));

exit:
    return ret;
}

static int32_t CTProcessLoader_retain(TProcessLoader *me)
{
    atomicAdd(&me->refs, 1);
    return Object_OK;
}

static int32_t CTProcessLoader_release(TProcessLoader *me)
{
    if (atomicAdd(&me->refs, -1) == 0) {
        Object_RELEASE_IF(me->credentials);
        HEAP_FREE_PTR(me);
    }

    return Object_OK;
}

static ITProcessLoader_DEFINE_INVOKE(CTProcessLoader_invoke, CTProcessLoader_, TProcessLoader *);

int32_t CTProcessLoader_open(uint32_t uid, Object credentials, Object *objOut)
{
    (void)uid;
    int32_t ret = Object_OK;
    TProcessLoader *me = HEAP_ZALLOC_TYPE(TProcessLoader);
    T_CHECK_ERR(me, Object_ERROR_MEM);

    me->refs = 1;
    me->cid = DL_OEM_SIGNED;
    Object_INIT(me->credentials, credentials);

    *objOut = (Object){CTProcessLoader_invoke, me};

exit:
    return ret;
}

int32_t CTProcessLoader_openEmbedded(uint32_t cid, Object credentials, Object *objOut)
{
    int32_t ret = Object_OK;
    TProcessLoader *me = HEAP_ZALLOC_TYPE(TProcessLoader);
    T_CHECK_ERR(me, Object_ERROR_MEM);

    me->refs = 1;
    me->cid = cid;
    Object_INIT(me->credentials, credentials);

    *objOut = (Object){CTProcessLoader_invoke, me};

exit:
    return ret;
}

void CTProcessLoader_enable(Object prelauncherObj)
{
    pthread_mutex_lock(&gLock);
    Object_ASSIGN(gPreLauncherObj, prelauncherObj);
    pthread_mutex_unlock(&gLock);
}

void CTProcessLoader_disable(void)
{
    pthread_mutex_lock(&gLock);
    Object_ASSIGN_NULL(gPreLauncherObj);
    pthread_mutex_unlock(&gLock);
}
