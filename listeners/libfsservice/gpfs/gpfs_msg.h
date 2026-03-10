// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __GPFS_MSG_H_
#define __GPFS_MSG_H_

#include <stdint.h>

#include "cmn.h"

#define GPFILE_SERVICE_BUF_LEN  (504*1024)
#define GPFILE_SERVICE_ID        0x7000

/**
 * @enum tz_gpfs_msg_cmd_type
 * @brief GP File system service commands used by QTEE to request services from the REE.
 *
 * These commands represent various Secure file operations that QTEE can request
 * from the Rich Execution Environment (REE).
 */
typedef enum {
	TZ_GPFS_MSG_CMD_DATA_FILE_READ = 0x00000004, /**< Read from a file in data partition */
	TZ_GPFS_MSG_CMD_DATA_FILE_WRITE, /**< Write to a file in data partition */
	TZ_GPFS_MSG_CMD_DATA_FILE_REMOVE, /**< Remove a file from data partition */
	TZ_GPFS_MSG_CMD_DATA_FILE_RENAME, /**< Rename a file in data partition */
	TZ_GPFS_MSG_CMD_PERSIST_FILE_READ, /**< Read from a file in persist partition */
	TZ_GPFS_MSG_CMD_PERSIST_FILE_WRITE, /**< Write to a file in persist partition */
	TZ_GPFS_MSG_CMD_PERSIST_FILE_REMOVE, /**< Remove a file from persist partition */
	TZ_GPFS_MSG_CMD_PERSIST_FILE_RENAME, /**< Rename a file in persist partition */
	TZ_GPFS_MSG_CMD_GPFS_VERSION, /**< Check the SFS version */
	TZ_GPFS_MSG_CMD_UNKNOWN = 0x7FFFFFFF
} tz_gpfs_msg_cmd_type;

#define TZ_GPFS_FILE_SIZE 500 * 1024

/**
 * @struct tz_gpfile_read_req_s
 * @brief Request structure for reading from a file.
 */
typedef struct tz_gpfile_read_req_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
	char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pathname of the file */
	int32_t offset; /**< Offset in the file to start reading from */
	uint32_t count; /**< Number of bytes to read */
} __attribute__((packed)) tz_gpfile_read_req_t;

/**
 * @struct tz_gpfile_read_rsp_s
 * @brief Response structure for reading from a file.
 */
typedef struct tz_gpfile_read_rsp_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
	int32_t err; /**< Error code (0 if success) */
	uint32_t num_bytes_read; /**< Number of bytes read (valid only if err is 0) */
	uint8_t buf[TZ_GPFS_FILE_SIZE]; /**< Buffer containing the read data */
} __attribute__((packed)) tz_gpfile_read_rsp_t;

/**
 * @struct tz_gpfile_write_req_s
 * @brief Request structure for writing to a file.
 */
typedef struct tz_gpfile_write_req_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
	char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pathname of the file */
	int32_t offset; /**< Offset in the file to start writing to */
	uint32_t count; /**< Number of bytes to write */
	uint32_t backup; /**< Whether to create a backup (non-zero to enable) */
	uint8_t buf[TZ_GPFS_FILE_SIZE]; /**< Buffer to write from (must be last, variable-length) */
} __attribute__((packed)) tz_gpfile_write_req_t;

/**
 * @struct tz_gpfile_write_rsp_s
 * @brief Response structure for writing to a file.
 */
typedef struct tz_gpfile_write_rsp_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
	int32_t err; /**< Error code (0 if success) */
	uint32_t num_bytes_written; /**< Number of bytes written (valid only if err is 0) */
} __attribute__((packed)) tz_gpfile_write_rsp_t;

/**
 * @struct tz_gpfile_remove_req_s
 * @brief Request structure for removing a file.
 */
typedef struct tz_gpfile_remove_req_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
	char pathname [TZ_CM_MAX_NAME_LEN]; /**< Pathname of the file or directory to remove */
} __attribute__((packed)) tz_gpfile_remove_req_t;

/**
 * @struct tz_gpfile_remove_rsp_s
 * @brief Response structure for removing a file.
 */
typedef struct tz_gpfile_remove_rsp_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
	int32_t err; /**< Error code (0 if success) */
} __attribute__((packed)) tz_gpfile_remove_rsp_t;

/**
 * @struct tz_gpfile_rename_req_s
 * @brief Request structure for renaming a file.
 */
typedef struct tz_gpfile_rename_req_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
	char from[TZ_CM_MAX_NAME_LEN]; /**< Current pathname of the file */
	char to[TZ_CM_MAX_NAME_LEN]; /**< New pathname of the file */
} __attribute__((packed)) tz_gpfile_rename_req_t;

/**
 * @struct tz_gpfile_rename_rsp_s
 * @brief Response structure for renaming a file.
 */
typedef struct tz_gpfile_rename_rsp_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
	int32_t err; /**< Error code (0 if success) */
} __attribute__((packed)) tz_gpfile_rename_rsp_t;

/**
 * @struct tz_gpfile_checkversion_req_s
 * @brief Request structure for checking the GPFS version.
 */
typedef struct tz_gpfile_checkversion_req_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
} __attribute__((packed)) tz_gpfile_checkversion_req_t;

/**
 * @struct tz_gpfile_version_rsp_s
 * @brief Response structure for GPFS version information.
 */
typedef struct tz_gpfile_version_rsp_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
	uint32_t version; /**< Current version of the GPFS implementation */
	int32_t err; /**< Error code (0 if success) */
} __attribute__((packed)) tz_gpfile_version_rsp_t;

/**
 * @struct tz_gpfile_err_rsp_s
 * @brief Generic error response structure.
 */
typedef struct tz_gpfile_err_rsp_s {
	tz_gpfs_msg_cmd_type cmd_id; /**< Command ID (first 4 bytes) */
	int32_t err; /**< Error code (0 if success) */
} __attribute__((packed)) tz_gpfile_err_rsp_t;

#endif //__GPFS_MSG_H_
