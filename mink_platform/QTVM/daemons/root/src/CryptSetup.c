// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifdef USE_SW_ENCRYPT

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "CVMDeviceUniqueKey.h"
#include "IClientEnv.h"
#include "IVMDeviceUniqueKey.h"
#include "TUtils.h"
#include "TZCom.h"

#define CRYPTSETUP_BIN "/usr/sbin/cryptsetup"
#define CRYPTSETUP_OPEN_OPTS "-q --type luks2"
#define CRYPTSETUP_FORMAT_OPTS \
    "--cipher=\"aes-cbc-essiv:sha256\" --type luks2 --integrity=\"hmac-sha256\" \
    --sector-size 4096 --pbkdf-force-iterations 4 --pbkdf-memory 32 --pbkdf-parallel 4"
#define CRYPTSETUP_BLOCK_DEV "/dev/vdb"
#define CRYPTSETUP_DM_NAME "persist"
#define CRYPTSETUP_DM_PATH "/dev/mapper/persist"
#define CRYPTSETUP_MEMORY_MB 30
#define CRYPTSETUP_KEY_SIZE 32
#define CRYPTSETUP_MAX_CMD_LEN 250

int32_t cryptsetup_smcinvoke_setup(Object *appObject, uint32_t UID)
{
    int32_t ret = Object_OK;
    Object clientEnv = Object_NULL;

    T_CALL(TZCom_getClientEnvObject(&clientEnv));
    T_CHECK(!Object_isNull(clientEnv));

    T_CALL(IClientEnv_open(clientEnv, UID, appObject));

exit:
    Object_ASSIGN_NULL(clientEnv);
    return ret;
}

int32_t cryptsetup_fetch_key(uint8_t **derived_key)
{
    Object appObject = Object_NULL;
    const size_t key_size = CRYPTSETUP_KEY_SIZE;
    size_t output_lenout = 0;
    int32_t ret = Object_OK;

    T_CALL(cryptsetup_smcinvoke_setup(&appObject, CVMDeviceUniqueKey_UID));

    *derived_key = (uint8_t *)malloc(sizeof(uint8_t) * key_size);
    T_CHECK(*derived_key != NULL);
    memset(*derived_key, 0, key_size);

    T_CALL(IVMDeviceUniqueKey_derive(appObject, *derived_key, key_size, &output_lenout));

exit:
    Object_ASSIGN_NULL(appObject);
    return ret;
}

int32_t run_cryptsetup(const char *command, uint8_t *derived_key)
{
    // run command and pass the derived key into stdin
    int32_t key_size = CRYPTSETUP_KEY_SIZE;
    FILE *procfd = NULL;
    int32_t ret = Object_OK;

    procfd = popen(command, "w");
    if (!procfd) {
        LOG_MSG("Failed to execute cryptsetup command: %s\n", strerror(errno));
        ret = Object_ERROR;
        goto exit;
    }
    if (fwrite(derived_key, sizeof(uint8_t), key_size, procfd) != key_size) {
        LOG_MSG("Failed to write %d bytes to cryptsetup\n", key_size);
        ret = Object_ERROR;
        // don't bail yet - try to close and cleanup the process
    }
    // Signal handler will intercept the return code. The process needs to
    //  finish before the thread continues but this thread can't react to
    //  any error code from cryptsetup.
    // TODO: Is there a way to handle the response?
    pclose(procfd);

exit:
    return ret;
}

int32_t do_cryptsetup()
{
    uint8_t *derived_key = NULL;
    int32_t ret = Object_OK;
    char cryptsetup_open[CRYPTSETUP_MAX_CMD_LEN];
    char cryptsetup_fmt[CRYPTSETUP_MAX_CMD_LEN];
    struct stat cryptsetupStat;

    // Sanity check for dependencies. Bail if unmet.
    if (-1 == stat(CRYPTSETUP_BIN, &cryptsetupStat)) {
        // cryptsetup binary must be installed
        LOG_MSG("Could not stat cryptsetup binary: %s", strerror(errno));
        return Object_ERROR;
    } else if (-1 == stat(CRYPTSETUP_BLOCK_DEV, &cryptsetupStat)) {
        // /dev/vdb is not available
        LOG_MSG("Could not stat %s: %s", CRYPTSETUP_BLOCK_DEV, strerror(errno));
        return Object_ERROR;
    } else if (0 == stat(CRYPTSETUP_DM_PATH, &cryptsetupStat)) {
        // A previous launch may have mounted /dev/mapper/persist
        LOG_MSG("/dev/mapper/persist is already created");
        return Object_OK;
    }

    // Drop CAP_IPC_LOCK to prevent cryptsetup from locking memory. Locking
    //  memory prevents memory reclaim from ocurring and may cause the device
    //  to OOM.
    T_GUARD(cap_drop_bound(CAP_IPC_LOCK));

    T_GUARD(cryptsetup_fetch_key(&derived_key));

    ret = snprintf(cryptsetup_open, CRYPTSETUP_MAX_CMD_LEN, "%s luksOpen %s %s %s", CRYPTSETUP_BIN,
                   CRYPTSETUP_BLOCK_DEV, CRYPTSETUP_DM_NAME, CRYPTSETUP_OPEN_OPTS);
    T_CHECK(ret > 0 && ret < CRYPTSETUP_MAX_CMD_LEN);

    ret = snprintf(cryptsetup_fmt, CRYPTSETUP_MAX_CMD_LEN, "%s luksFormat %s %s %s", CRYPTSETUP_BIN,
                   CRYPTSETUP_BLOCK_DEV, CRYPTSETUP_OPEN_OPTS, CRYPTSETUP_FORMAT_OPTS);
    T_CHECK(ret > 0 && ret < CRYPTSETUP_MAX_CMD_LEN);

    T_GUARD(run_cryptsetup((const char *)cryptsetup_open, derived_key));

    ret = stat(CRYPTSETUP_DM_PATH, &cryptsetupStat);
    if (ret == -1 && errno == ENOENT) {
        // on first boot, a failure is expected. Format now
        LOG_MSG("Reformatting persist partition");
        T_GUARD(run_cryptsetup((const char *)cryptsetup_fmt, derived_key));

        T_GUARD(run_cryptsetup((const char *)cryptsetup_open, derived_key));
        ret = stat(CRYPTSETUP_DM_PATH, &cryptsetupStat);
        if (ret == -1) {
            LOG_MSG("Could not find /dev/mapper/persist: %s", strerror(errno));
            ret = Object_ERROR;
        } else {
            LOG_MSG("/dev/mapper/persist successfully found");
        }
    } else if (ret == -1) {
        LOG_MSG("Could not stat %s: %s", CRYPTSETUP_BLOCK_DEV, strerror(errno));
        ret = Object_ERROR;
        goto exit;
    } else {
        LOG_MSG("Persist partition already formatted");
    }

exit:
    if (derived_key) {
        free(derived_key);
    }
    return ret;
}

#endif  // USE_SW_ENCRYPT
