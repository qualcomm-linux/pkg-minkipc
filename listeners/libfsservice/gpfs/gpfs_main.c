// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "gpfs_msg.h"
#include "cmn.h"
#include "helper.h"
#include "gpfs.h"

#include "CListenerCBO.h"
#include "CRegisterListenerCBO.h"
#include "IRegisterListenerCBO.h"
#include "IClientEnv.h"
#include "MinkCom.h"

/* Exported init functions */
int init(void);
void deinit(void);

int smci_dispatch(void *buf, size_t buf_len);

static Object register_obj = Object_NULL;
static Object cbo = Object_NULL;
static Object mo = Object_NULL;

int init(void)
{
	int ret = 0;
	int32_t rv = Object_OK;

	Object root = Object_NULL;
	Object client_env = Object_NULL;
	void *buf = NULL;
	size_t buf_len = 0;

	/* There are 4 threads for each callback. */
	rv = MinkCom_getRootEnvObject(&root);
	if (Object_isERROR(rv)) {
		root = Object_NULL;
		MSGE("getRootEnvObject failed: 0x%x\n", rv);
		ret = -1;
		goto err;
	}

	rv = MinkCom_getClientEnvObject(root, &client_env);
	if (Object_isERROR(rv)) {
		client_env = Object_NULL;
		MSGE("getClientEnvObject failed: 0x%x\n", rv);
		ret = -1;
		goto err;
	}

	rv = IClientEnv_open(client_env, CRegisterListenerCBO_UID,
			     &register_obj);
	if (Object_isERROR(rv)) {
		register_obj = Object_NULL;
		MSGE("IClientEnv_open failed: 0x%x\n", rv);
		ret = -1;
		goto err;
	}

	rv = MinkCom_getMemoryObject(root, GPFILE_SERVICE_BUF_LEN, &mo);
	if (Object_isERROR(rv)) {
		mo = Object_NULL;
		ret = -1;
		MSGE("getMemoryObject failed: 0x%x", rv);
		goto err;
	}

	rv = MinkCom_getMemoryObjectInfo(mo, &buf, &buf_len);
	if (Object_isERROR(rv)) {
		ret = -1;
		MSGE("getMemoryObjectInfo failed: 0x%x\n", rv);
		goto err;
	}

	/* Create CBO listener and register it */
	rv = CListenerCBO_new(&cbo, GPFILE_SERVICE_ID, smci_dispatch, buf, buf_len);
	if (Object_isERROR(rv)) {
		cbo = Object_NULL;
		ret = -1;
		MSGE("CListenerCBO_new failed: 0x%x\n", rv);
		goto err;
	}

	rv = IRegisterListenerCBO_register(register_obj,
					   GPFILE_SERVICE_ID,
					   cbo,
					   mo);
	if (Object_isERROR(rv)) {
		ret = -1;
		MSGE("IRegisterListenerCBO_register(%d) failed: 0x%x",
		     GPFILE_SERVICE_ID, rv);
		goto err;
	}

	Object_ASSIGN_NULL(client_env);
	Object_ASSIGN_NULL(root);

	return ret;

err:
	Object_ASSIGN_NULL(cbo);
	Object_ASSIGN_NULL(mo);
	Object_ASSIGN_NULL(register_obj);
	Object_ASSIGN_NULL(client_env);
	Object_ASSIGN_NULL(root);

	return ret;
}

void deinit(void)
{
	Object_ASSIGN_NULL(register_obj);
	Object_ASSIGN_NULL(cbo);
	Object_ASSIGN_NULL(mo);
}

int smci_dispatch(void *buf, size_t buf_len)
{
	int ret = -1;
	tz_gpfs_msg_cmd_type gpfs_cmd_id;
	MSGD("GPFSDispatch starts!\n");

	/* Buffer size is 4K aligned and should always be big enough to
	 * accomodate the largest of gpfs structs */
	if (buf_len < TZ_GP_MAX_BUF_LEN) {
		MSGE("[%s:%d] Invalid buffer len.\n", __func__, __LINE__);
		return -1; // This should be reported as a transport error
	}

	gpfs_cmd_id = *((tz_gpfs_msg_cmd_type *)buf);
	MSGD("gpfs_cmd_id = 0x%x\n", gpfs_cmd_id);

	/* Make sure that partition is mounted before proceeding */
	if (gpfs_cmd_id != TZ_GPFS_MSG_CMD_GPFS_VERSION &&
	    !is_persist_partition_mounted()) {
		MSGE("persist partition is not mounted, dispatch failed!\n");
		gpfile_partition_error(buf, buf_len);
		return 0;
	}

	/* Read command id */
	switch (gpfs_cmd_id) {
	case (TZ_GPFS_MSG_CMD_DATA_FILE_READ):
	case (TZ_GPFS_MSG_CMD_PERSIST_FILE_READ):
		MSGD("gpfile_read starts!\n");
		ret = gpfile_read(buf, buf_len, buf, buf_len);
		MSGD("gpfile_read finished!\n");
		break;
	case (TZ_GPFS_MSG_CMD_DATA_FILE_WRITE):
	case (TZ_GPFS_MSG_CMD_PERSIST_FILE_WRITE):
		MSGD("gpfile_write starts!\n");
		ret = gpfile_write(buf, buf_len, buf, buf_len);
		MSGD("gpfile_write finished!\n");
		break;
	case (TZ_GPFS_MSG_CMD_DATA_FILE_REMOVE):
	case (TZ_GPFS_MSG_CMD_PERSIST_FILE_REMOVE):
		MSGD("gpfile_remove starts!\n");
		ret = gpfile_remove(buf, buf_len, buf, buf_len);
		MSGD("gpfile_remove finished!\n");
		break;
	case (TZ_GPFS_MSG_CMD_DATA_FILE_RENAME):
	case (TZ_GPFS_MSG_CMD_PERSIST_FILE_RENAME):
		MSGD("gpfile_rename starts!\n");
		ret = gpfile_rename(buf, buf_len, buf, buf_len);
		MSGD("gpfile_rename finished!\n");
		break;
	case (TZ_GPFS_MSG_CMD_GPFS_VERSION):
		MSGD("gpfile_check_version starts!\n");
		ret = gpfile_check_version(buf, buf_len, buf,
					   buf_len);
		MSGD("gpfile_check_version finished!\n");
		break;
	default:
		MSGE("gp file command %d is not found!, returning ERROR!\n",
		     gpfs_cmd_id);
		ret = gpfile_error(buf, buf_len);
		break;
	}

	MSGD("GPFSDispatch ends %d\n", ret);
	return ret;
}
