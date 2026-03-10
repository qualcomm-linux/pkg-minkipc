// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "fs_msg.h"
#include "cmn.h"
#include "helper.h"
#include "fs.h"

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

	rv = MinkCom_getMemoryObject(root, FILE_SERVICE_BUF_LEN, &mo);
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
	rv = CListenerCBO_new(&cbo, FILE_SERVICE_ID, smci_dispatch, buf, buf_len);
	if (Object_isERROR(rv)) {
		cbo = Object_NULL;
		ret = -1;
		MSGE("CListenerCBO_new failed: 0x%x\n", rv);
		goto err;
	}

	rv = IRegisterListenerCBO_register(register_obj,
					   FILE_SERVICE_ID,
					   cbo,
					   mo);
	if (Object_isERROR(rv)) {
		ret = -1;
		MSGE("IRegisterListenerCBO_register(%d) failed: 0x%x",
		     FILE_SERVICE_ID, rv);
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
	tz_fs_msg_cmd_type fs_cmd_id;

	MSGD("FSDispatch starts!\n");

	/* Buffer size is 4K aligned and should always be big enough to
	 * accomodate the largest of fs structs */
	if (buf_len < TZ_MAX_BUF_LEN) {
		MSGE("[%s:%d] Invalid buffer len.\n", __func__, __LINE__);
		return -1; // This should be reported as a transport error
	}

	fs_cmd_id = *((tz_fs_msg_cmd_type *)buf);
	MSGD("fs_cmd_id = 0x%x\n", fs_cmd_id);

	/* Make sure that partition is mounted before proceeding */
	if (!is_persist_partition_mounted()) {
		MSGE("persist partition is not mounted, dispatch failed!\n.");
		file_partition_error(fs_cmd_id, buf);
		return 0;
	}

	/* Read command id */
	switch (fs_cmd_id) {
	case TZ_FS_MSG_CMD_FILE_OPEN:
		MSGD("file_open starts!\n");
		ret = file_open(buf, buf_len, buf, buf_len);
		MSGD("file_open finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_OPENAT:
		MSGD("file_openat starts!\n");
		ret = file_openat(buf, buf_len, buf, buf_len);
		MSGD("file_openat finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_UNLINKAT:
		MSGD("file_unlinkat starts!\n");
		ret = file_unlinkat(buf, buf_len, buf, buf_len);
		MSGD("file_unlinkatis finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_FCNTL:
		MSGD("file_fcntl starts!\n");
		ret = file_fcntl(buf, buf_len, buf, buf_len);
		MSGD("file_fcntl finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_CREAT:
		MSGD("file_create starts!\n");
		ret = file_creat(buf, buf_len, buf, buf_len);
		MSGD("file_create finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_READ:
		MSGD("file_read starts!\n");
		ret = file_read(buf, buf_len, buf, buf_len);
		MSGD("file_read finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_WRITE:
		MSGD("file_write starts!\n");
		ret = file_write(buf, buf_len, buf, buf_len);
		MSGD("file_write finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_CLOSE:
		MSGD("file_close starts!\n");
		ret = file_close(buf, buf_len, buf, buf_len);
		MSGD("file_close finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_LSEEK:
		MSGD("file_lseek starts!\n");
		ret = file_lseek(buf, buf_len, buf, buf_len);
		MSGD("file_lseek finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_LINK:
		MSGD("file_link starts!\n");
		ret = file_link(buf, buf_len, buf, buf_len);
		MSGD("file_link finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_UNLINK:
		MSGD("file_unlink starts\n");
		ret = file_unlink(buf, buf_len, buf, buf_len);
		MSGD("file_unlink finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_RMDIR:
		MSGD("file_rmdir starts!\n");
		ret = file_rmdir(buf, buf_len, buf, buf_len);
		MSGD("file_rmdir finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_FSTAT:
		MSGD("file_fstat starts!\n");
		ret = file_fstat(buf, buf_len, buf, buf_len);
		MSGD("file_fstat finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_LSTAT:
		MSGD("file_lstat starts!\n");
		ret = file_lstat(buf, buf_len, buf, buf_len);
		MSGD("file_lstat finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_MKDIR:
		MSGD("file_mkdir starts!\n");
		ret = file_mkdir(buf, buf_len, buf, buf_len);
		MSGD("file_mkdir finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_TESTDIR:
		MSGD("file_testdir starts!\n");
		ret = file_testdir(buf, buf_len, buf, buf_len);
		MSGD("file_testdir finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_TELLDIR:
		MSGD("file_telldir starts!\n");
		ret = file_telldir(buf, buf_len, buf, buf_len);
		MSGD("file_telldir finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_REMOVE:
		MSGD("file_remove starts!\n");
		ret = file_remove(buf, buf_len, buf, buf_len);
		MSGD("file_remove finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_CHOWN_CHMOD:
		MSGD("file_dir_chown_chmod starts!\n");
		ret = file_dir_chown_chmod(buf, buf_len, buf,
					   buf_len);
		MSGD("file_dir_chown_chmod finished!\n");
		break;
	case TZ_FS_MSG_CMD_FILE_END:
		MSGD("file_services Dispatch end request!\n");
		ret = file_end(buf, buf_len, buf, buf_len);
		break;
	case TZ_FS_MSG_CMD_FILE_SYNC:
		MSGD("file_sync starts!\n");
		ret = file_sync(buf, buf_len, buf, buf_len);
		MSGD("file_sync finished!\n");
		break;

	case TZ_FS_MSG_CMD_FILE_RENAME:
		MSGD("file_rename starts!\n");
		ret = file_rename(buf, buf_len, buf, buf_len);
		MSGD("file_rename finished!\n");
		break;

	case TZ_FS_MSG_CMD_FILE_PAR_FR_SIZE:
		MSGD("file_get_partition_free_size get partition free size\n");
		ret = file_get_partition_free_size(buf, buf_len, buf,
					     buf_len);
		MSGD("file_get_partition_free_size finished!\n");
		break;

	case TZ_FS_MSG_CMD_DIR_OPEN:
		MSGD("dir_open starts!\n");
		ret = dir_open(buf, buf_len, buf, buf_len);
		MSGD("dir_open finished!\n");
		break;

	case TZ_FS_MSG_CMD_DIR_READ:
		MSGD("dir_read starts!\n");
		ret = dir_read(buf, buf_len, buf, buf_len);
		MSGD("dir_read finished!\n");
		break;

	case TZ_FS_MSG_CMD_DIR_CLOSE:
		MSGD("dir_close starts!\n");
		ret = dir_close(buf, buf_len, buf, buf_len);
		MSGD("dir_close finished!\n");
		break;

	case TZ_FS_MSG_CMD_FILE_GET_ERRNO:
		MSGD("file_get_errno starts!\n");
		ret = file_get_errno(buf, buf_len);
		MSGD("file_get_errno finished\n");
		break;
	default:
		MSGD("file command 0x%x is not found!, returning ERROR!\n",
		     fs_cmd_id);
		ret = file_error(buf, buf_len);
		break;
	}

	MSGD("FSDispatch ends! %d\n", ret);
	return ret;
}
