// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

// "Tunnel Invoke Service" would run in a VM which needs to tunnel into TZ.
//   It facilitates the creation and destruction of the tunnel.
//   It exposes "getClientEnv" which allows client applications to use the tunnel.

#include "TunnelInvokeService.h"
#include <stdint.h>
#include "CTunnelInvokeGetPeer_TIFS.h"
#include "CTunnelInvokeMgr.h"
#include "IClientEnv.h"
#include "IOpener.h"
#include "ITunnelInvokeGetPeer.h"
#include "ITunnelInvokeMgr.h"
#include "TICrypto.h"
#include "TZCom.h"
#include "TunnelInvoke.h"
#include "TunnelInvokeForwarderService.h"
#include "object.h"
#include "ssgtzd_logging.h"

#define OBJ_TABLE_LEN 256

// Tunnel Invoke Service supports one session
static TI* gTi = NULL;
static uint64_t gSessionID = 0;
static Object gClientEnvTunnel = Object_NULL;

#define CHECK_BAIL(rv)        \
    if (Object_isERROR(rv)) { \
        goto bail;            \
    }

// Toggle on and off for "no newSession" test build behavior
// These conditionals will be removed when the TVM has SMCI support
//#define NO_NEW_SESSION_ENG_TEST_BUILD

int32_t TunnelInvokeService_init(Object opener)
{
    int32_t rv = Object_ERROR;

    uint8_t key[TI_KEY_LEN_BYTES] = {0};
    size_t keyLenOut = 0;

    uint32_t tiVersion = TI_LATEST_VERSION;

    Object rootEnv = Object_NULL;
    Object clientEnv = Object_NULL;
    Object peer = Object_NULL;
    Object tiMgr = Object_NULL;
    Object getPeer = Object_NULL;
    Object openerTIFS = Object_NULL;

#ifdef NO_NEW_SESSION_ENG_TEST_BUILD
    {
        // Pretend we got a key:
        keyLenOut = TI_KEY_LEN_BYTES;
    }
#else
    {
        rv = TZCom_getClientEnvObject(&clientEnv);
        if (rv != Object_OK) {
            LOGE("Error %d getting clientEnv via SMCInvoke\n", rv);
            goto bail;
        }

        rv = IClientEnv_open(clientEnv, CTunnelInvokeMgr_UID, &tiMgr);
        if (Object_isERROR(rv)) {
            tiMgr = Object_NULL;
            goto bail;
        }

        // Create new sesson
        rv = ITunnelInvokeMgr_newSession(tiMgr, tiVersion, &gSessionID, key, sizeof(key),
                                         &keyLenOut);
        if (Object_isERROR(rv)) {
            gSessionID = 0;
            goto bail;
        }
    }
#endif

    // Open the getPeer service from the forwarder service (not directly from TZ).
    rv = IOpener_open(opener, CTunnelInvokeGetPeer_TIFS_UID, &getPeer);
    if (Object_isERROR(rv)) {
        LOGE("Error %d calling IOpener_open", rv);
        getPeer = Object_NULL;
        goto bail;
    }

    // Create TI endpoint
    rv = TI_new(tiVersion, false, key, keyLenOut, OBJ_TABLE_LEN, &gTi);
    if (Object_isERROR(rv)) {
        gTi = NULL;
        goto bail;
    }

    // Get TI peer from TZ
    rv = ITunnelInvokeGetPeer_getPeer(getPeer, gSessionID, &peer);
    if (Object_isERROR(rv)) {
        peer = Object_NULL;
        goto bail;
    }

    // Set peers on both tunnel endpoints
    TI_setPeer(gTi, peer);

    rv = TI_registerForCallbacks(gTi);
    CHECK_BAIL(rv);

    // Get the root obj operating through the tunnel
    rv = TI_newRemoteObject(gTi, TI_CENV_OBJ_INDEX, &gClientEnvTunnel);
    if (Object_isERROR(rv)) {
        gClientEnvTunnel = Object_NULL;
        goto bail;
    }

    rv = Object_OK;

bail:

    Object_ASSIGN_NULL(peer);
    Object_ASSIGN_NULL(tiMgr);
    Object_ASSIGN_NULL(getPeer);
    Object_ASSIGN_NULL(clientEnv);
    Object_ASSIGN_NULL(rootEnv);

    if (Object_isERROR(rv)) {
        LOGE("Oops! some error %d: calling tunnel deinit", rv);
        TunnelInvokeService_deinit();
    }

    return rv;
}

// Pre req: Release all objects opened through tunnel's clientEnv
//
// Upon failure, Tunnel Invoke Service may no longer be usable and
// there may be resource leaks
int32_t TunnelInvokeService_deinit()
{
    int32_t rv = Object_ERROR;
    Object tiMgr = Object_NULL;
    Object clientEnv = Object_NULL;
    Object rootEnv = Object_NULL;

    // Release static clientEnv
    Object_ASSIGN_NULL(gClientEnvTunnel);

#ifdef NO_NEW_SESSION_ENG_TEST_BUILD
    {
        // Nothing in this block

        // Not calling endSession, so clean up is not finished
        // and some resources will be left not released.
    }
#else
    {
        rv = TZCom_getClientEnvObject(&clientEnv);
        if (rv != Object_OK) {
            LOGE("Error %d getting clientEnv via SMCInvoke\n", rv);
            goto bail;
        }

        rv = IClientEnv_open(clientEnv, CTunnelInvokeMgr_UID, &tiMgr);
        if (Object_isERROR(rv)) {
            tiMgr = Object_NULL;
            goto bail;
        }

        // End session
        if (gSessionID) {
            rv = ITunnelInvokeMgr_endSession(tiMgr, gSessionID);
            CHECK_BAIL(rv);
            gSessionID = 0;
        }
    }
#endif

    // TI_releasePeer should delete the remote TI endpoint
    // TI_release should delete the local TI endpoint
    if (gTi) {
        TI_releasePeer(gTi);
        TI_release(gTi);
        gTi = NULL;
    }

    rv = Object_OK;

// Compiler complains about unused label...
#ifndef NO_NEW_SESSION_ENG_TEST_BUILD
bail:
#endif

    Object_ASSIGN_NULL(tiMgr);
    Object_ASSIGN_NULL(clientEnv);
    Object_ASSIGN_NULL(rootEnv);

    return rv;
}

Object TunnelInvokeService_getClientEnv()
{
    // Create a forwarder of the already existing clientEnv

    // In our case, since everything is in the same process,
    // just create a new reference

    Object obj = Object_NULL;
    Object_INIT(obj, gClientEnvTunnel);
    return obj;
}

int32_t TunnelInvokeService_open(uint32_t uid, Object* objOut)
{
    int32_t ret = Object_OK;

    ret = IClientEnv_open(gClientEnvTunnel, uid, objOut);
    if (Object_isERROR(ret)) {
        *objOut = Object_NULL;
        goto exit;
    }

exit:
    return ret;
}
