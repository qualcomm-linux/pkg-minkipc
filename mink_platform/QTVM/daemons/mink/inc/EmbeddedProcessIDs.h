// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __EMBEDDEDPROCESSID_H
#define __EMBEDDEDPROCESSID_H

#include <stdbool.h>
#include "libcontainer.h"

#ifndef OEM_VM
#ifndef OFFTARGET
#include "CDemuraTnService.h"
#include "CSecureDSPService.h"
#include "CTCDriverCBService.h"
#include "CTC2PAService.h"
#include "CTUICoreService.h"
#include "CTouchInput.h"
#include "CVMFileTransferService.h"
#include "CIPPService.h"
#include "CIDVService.h"
#endif

// OEM VM does not have any embedded services. It must open them from QTVM.
#ifdef QTVM_TEST
#include "CEmbeddedAllPrivilegeTestService.h"
#include "CEmbeddedCommonTestService.h"
#include "CEmbeddedLateRegistration.h"
#include "CEmbeddedMissingNeverUnload.h"
#include "CEmbeddedNormalDeathTestService.h"
#include "CEmbeddedSpareTestService.h"
#include "CEmbeddedStressTestService.h"
#include "CEmbeddedWrongTestService.h"
#include "CEmbeddedTVMSMCInvokeTestApp.h"
#include "CEmbeddedIPPTestApp.h"
#endif
#endif

typedef struct {
    uint32_t uid;
    cid_t cid;
} EmbeddedProcessID;

static EmbeddedProcessID embeddedProcessIDList[] = {
    {DAEMON_PRELAUNCHER, DAEMON_PRELAUNCHER},
    {DAEMON_MINK, DAEMON_MINK},
    {DL_OEM_SIGNED, DL_OEM_SIGNED},
#ifndef OEM_VM
#ifndef OFFTARGET
    {CTUICoreService_UID, TUI_CORE_SERVICE},
    {CTouchInput_UID, TUI_TRUSTED_INPUT},
    {CTCDriverCBService_UID, TRUSTED_CAMERA_CB_SERVICE},
    {CSecureDSPService_UID, DSP_SERVICE},
    {CVMFileTransferService_UID, VM_FILE_TRANSFER_SERVICE},
    {CDemuraTnService_UID, DEMURATN_SERVICE},
    {CIPPService_UID, IPP_SERVICE},
    {CTC2PAService_UID, C2PA_SERVICE},
    {CIDVService_UID, IDV_SERVICE},
#endif
#ifdef QTVM_TEST
    {CEmbeddedAllPrivilegeTestService_UID, TEST_ALL_PRIV},
    {CEmbeddedCommonTestService_UID, TEST_COMMON},
    {CEmbeddedStressTestService_UID, TEST_STRESS},
    {CEmbeddedNormalDeathTestService_UID, TEST_NORMAL_DEATH},
    {CEmbeddedSpareTestService_UID, TEST_SPARE},
    // This UID intentially doesn't match the service
    {CEmbeddedWrongTestService_UID, TEST_SPARE},
    {CEmbeddedMissingNeverUnload_UID, TEST_MISSING_NEVER_UNLOAD},
    {CEmbeddedLateRegistration_UID, TEST_LATE_REGISTRATION},
    {CEmbeddedTVMSMCInvokeTestApp_UID, TEST_SMCINVOKE},
    {CEmbeddedIPPTestApp_UID, TEST_IPP},
#endif
#endif
};

static size_t embeddedProcessIDCount = (sizeof(embeddedProcessIDList) / sizeof(EmbeddedProcessID));

#endif  // __EMBEDDEDPROCESSID_H
