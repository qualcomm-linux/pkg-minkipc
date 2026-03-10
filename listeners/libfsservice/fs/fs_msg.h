// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __FS_MSG_H_
#define __FS_MSG_H_

#include <stdint.h>

#include "cmn.h"

#define FILE_SERVICE_BUF_LEN    (20*1024)
#define FILE_SERVICE_ID          0xa

/**
 * @struct tz_stat
 * @brief Status structure for returning file status to QTEE
 */
typedef struct tz_stat {
	uint64_t st_dev; /**< ID of device containing file */
	unsigned char __pad0[4];
	uint32_t __st_ino;
	unsigned int st_mode; /**< protection */
	unsigned int st_nlink; /**< number of hard links */
	uint32_t st_uid; /**< user ID of owner */
	uint32_t st_gid; /**< group ID of owner */
	uint64_t st_rdev; /**< device ID (if special file) */
	unsigned char __pad3[4];
	int64_t st_size; /**< total size, in bytes */
	uint32_t st_blksize; /**< blocksize for filesystem I/O */
	uint64_t st_blocks; /**< number of blocks allocated */
	uint32_t st_atim; /**< time of last access */
	uint32_t st_atim_nsec;
	uint32_t st_mtim; /**< time of last modification */
	uint32_t st_mtim_nsec;
	uint32_t st_ctim; /**< time of last status change */
	uint32_t st_ctim_nsec;
	uint64_t st_ino; /**< inode number */
} __attribute__((packed)) tz_stat_t;

typedef struct tz_dirent {
	uint64_t d_ino;
	int64_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[TZ_CM_MAX_NAME_LEN];
} __attribute__((packed)) tz_dirent_t;

/**
 * @enum tz_fs_msg_cmd_type
 * @brief File system service commands used by QTEE to request services from the REE.
 *
 * These commands represent various file and directory operations that QTEE can
 * request from the Rich Execution Environment (REE).
 */
typedef enum {
	TZ_FS_MSG_CMD_FILE_START = 0x00000201,
	TZ_FS_MSG_CMD_FILE_OPEN,
	TZ_FS_MSG_CMD_FILE_OPENAT,
	TZ_FS_MSG_CMD_FILE_UNLINKAT,
	TZ_FS_MSG_CMD_FILE_FCNTL,
	TZ_FS_MSG_CMD_FILE_CREAT,
	TZ_FS_MSG_CMD_FILE_READ, /**< Read from a file */
	TZ_FS_MSG_CMD_FILE_WRITE, /**< Write to a file */
	TZ_FS_MSG_CMD_FILE_CLOSE, /**< Close a file opened for read/write */
	TZ_FS_MSG_CMD_FILE_LSEEK, /**< Seek to a offset in file */
	TZ_FS_MSG_CMD_FILE_LINK,
	TZ_FS_MSG_CMD_FILE_UNLINK,
	TZ_FS_MSG_CMD_FILE_RMDIR,
	TZ_FS_MSG_CMD_FILE_FSTAT,
	TZ_FS_MSG_CMD_FILE_LSTAT,
	TZ_FS_MSG_CMD_FILE_MKDIR,
	TZ_FS_MSG_CMD_FILE_TESTDIR,
	TZ_FS_MSG_CMD_FILE_TELLDIR,
	TZ_FS_MSG_CMD_FILE_REMOVE,
	TZ_FS_MSG_CMD_FILE_CHOWN_CHMOD,
	TZ_FS_MSG_CMD_FILE_UNUSED,
	TZ_FS_MSG_CMD_FILE_SYNC,
	TZ_FS_MSG_CMD_FILE_RENAME,
	TZ_FS_MSG_CMD_FILE_PAR_FR_SIZE, /**< get partition free size */
	TZ_FS_MSG_CMD_DIR_OPEN,
	TZ_FS_MSG_CMD_DIR_READ,
	TZ_FS_MSG_CMD_DIR_CLOSE,
	TZ_FS_MSG_CMD_FILE_GET_ERRNO,
	TZ_FS_MSG_CMD_FILE_END,
	TZ_FS_MSG_CMD_UNKNOWN = 0x7FFFFFFF
} tz_fs_msg_cmd_type;

typedef enum
{
    E_FS_SUCCESS         =  0,
    E_FS_FAILURE         = -1,
    E_FS_INVALID_ARG     = -2,
    E_FS_DIR_NOT_EXIST   = -3,
    E_FS_PATH_TOO_LONG   = -4,
} tz_common_error_codes;

/**
 * @struct tz_file_open_req_s
 * @brief Command structure for file open
 */
typedef struct tz_file_open_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char pathname [TZ_CM_MAX_NAME_LEN]; /**< Pointer to file name with complete path */
	int flags; /**< File status flags */
} __attribute__((packed)) tz_file_open_req_t;

/**
 * @struct tz_file_open_rsp_s
 * @brief Response structure for file open
 */
typedef struct tz_file_open_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_open_rsp_t;

/**
 * @struct tz_file_openat_req_s
 * @brief Command structure for file openat
 */
typedef struct tz_file_openat_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int dirfd; /**< Directory file descriptor */
	char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pointer to file name with complete path */
	int flags; /**< File status flags */
} __attribute__((packed)) tz_file_openat_req_t;

/**
 * @struct tz_file_openat_rsp_s
 * @brief Response structure for file openat
 */
typedef struct tz_file_openat_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_openat_rsp_t;

/**
 * @struct tz_file_unlinkat_req_s
 * @brief Command structure for file unlinkat
 */
typedef struct tz_file_unlinkat_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int dirfd; /**< File descriptor */
	char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pathname of file */
	int flags; /**< Flags */
} __attribute__((packed)) tz_file_unlinkat_req_t;

/**
 * @struct tz_file_unlinkat_rsp_s
 * @brief Response structure for file unlinkat
 */
typedef struct tz_file_unlinkat_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_unlinkat_rsp_t;

/**
 * @struct tz_file_fcntl_req_s
 * @brief Command structure for file fcntl
 */
typedef struct tz_file_fcntl_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int fd; /**< File descriptor */
	int cmd; /**< Operation to be performed */
} __attribute__((packed)) tz_file_fcntl_req_t;

/**
 * @struct tz_file_fcntl_rsp_s
 * @brief Response structure for file fcntl
 */
typedef struct tz_file_fcntl_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Return value depends on operation */
} __attribute__((packed)) tz_file_fcntl_rsp_t;

/**
 * @struct tz_file_creat_req_s
 * @brief Command structure for file creat
 */
typedef struct tz_file_creat_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pathname of file */
	uint32_t mode; /**< Access modes */
} __attribute__((packed)) tz_file_creat_req_t;

/**
 * @struct tz_file_creat_rsp_s
 * @brief Response structure for file creat
 */
typedef struct tz_file_creat_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_creat_rsp_t;

/**
 * @struct tz_file_read_req_s
 * @brief Command structure for file read
 */
typedef struct tz_file_read_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int fd; /**< File descriptor */
	uint32_t count; /**< Number of bytes to read */
} __attribute__((packed)) tz_file_read_req_t;

/**
 * @struct tz_file_read_rsp_s
 * @brief Response structure for file read
 */
typedef struct tz_file_read_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	uint8_t buf[TZ_CM_MAX_DATA_LEN]; /**< Buffer containing read data */
	int32_t ret; /**< Number of bytes read */
} __attribute__((packed)) tz_file_read_rsp_t;

/**
 * @struct tz_file_write_req_s
 * @brief Command structure for file write
 */
typedef struct tz_file_write_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int fd; /**< File descriptor */
	uint8_t buf[TZ_CM_MAX_DATA_LEN]; /**< Buffer to write from */
	uint32_t count; /**< Number of bytes to write */
} __attribute__((packed)) tz_file_write_req_t;

/**
 * @struct tz_file_write_rsp_s
 * @brief Response structure for file write
 */
typedef struct tz_file_write_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int32_t ret; /**< Number of bytes written */
} __attribute__((packed)) tz_file_write_rsp_t;

/**
 * @struct tz_file_close_req_s
 * @brief Command structure for file close
 */
typedef struct tz_file_close_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int fd; /**< File descriptor */
} __attribute__((packed)) tz_file_close_req_t;

/**
 * @struct tz_file_close_rsp_s
 * @brief Response structure for file close
 */
typedef struct tz_file_close_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_close_rsp_t;

/**
 * @struct tz_file_lseek_req_s
 * @brief Command structure for file lseek
 */
typedef struct tz_file_lseek_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int fildes; /**< File descriptor */
	int32_t offset; /**< New offset */
	int whence; /**< Directive */
} __attribute__((packed)) tz_file_lseek_req_t;

/**
 * @struct tz_file_lseek_rsp_s
 * @brief Response structure for file lseek
 */
typedef struct tz_file_lseek_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int32_t ret; /**< Resulting offset */
} __attribute__((packed)) tz_file_lseek_rsp_t;

/**
 * @struct tz_file_link_req_s
 * @brief Command structure for file link
 */
typedef struct tz_file_link_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char oldpath[TZ_CM_MAX_NAME_LEN]; /**< Pathname of existing file */
	char newpath[TZ_CM_MAX_NAME_LEN]; /**< Pathname of new link to existing file */
} __attribute__((packed)) tz_file_link_req_t;

/**
 * @struct tz_file_link_rsp_s
 * @brief Response structure for file link
 */
typedef struct tz_file_link_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_link_rsp_t;

/**
 * @struct tz_file_unlink_req_s
 * @brief Command structure for file unlink
 */
typedef struct tz_file_unlink_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pathname of file */
} __attribute__((packed)) tz_file_unlink_req_t;

/**
 * @struct tz_file_unlink_rsp_s
 * @brief Response structure for file unlink
 */
typedef struct tz_file_unlink_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_unlink_rsp_t;

/**
 * @struct tz_file_rmdir_req_s
 * @brief Command structure for file rmdir
 */
typedef struct tz_file_rmdir_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char path[TZ_CM_MAX_NAME_LEN]; /**< Pathname of file */
} __attribute__((packed)) tz_file_rmdir_req_t;

/**
 * @struct tz_file_rmdir_rsp_s
 * @brief Response structure for file rmdir
 */
typedef struct tz_file_rmdir_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_rmdir_rsp_t;

/**
 * @struct tz_file_fstat_req_s
 * @brief Command structure for file fstat
 */
typedef struct tz_file_fstat_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int filedes; /**< File descriptor */
} __attribute__((packed)) tz_file_fstat_req_t;

/**
 * @struct tz_file_fstat_rsp_s
 * @brief Response structure for file fstat
 */
typedef struct tz_file_fstat_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	struct tz_stat buf; /**< Pointer to status structure */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_fstat_rsp_t;

/**
 * @struct tz_file_lstat_req_s
 * @brief Command structure for file lstat
 */
typedef struct tz_file_lstat_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char path[TZ_CM_MAX_NAME_LEN]; /**< Pathname of file */
} __attribute__((packed)) tz_file_lstat_req_t;

/**
 * @struct tz_file_lstat_rsp_s
 * @brief Response structure for file lstat
 */
typedef struct tz_file_lstat_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	struct tz_stat buf; /**< Pointer to status structure */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_lstat_rsp_t;

/**
 * @struct tz_file_mkdir_req_s
 * @brief Command structure for file mkdir
 */
typedef struct tz_file_mkdir_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pathname of directory */
	uint32_t mode; /**< Permissions mode */
} __attribute__((packed)) tz_file_mkdir_req_t;

/**
 * @struct tz_file_mkdir_rsp_s
 * @brief Response structure for file mkdir
 */
typedef struct tz_file_mkdir_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_mkdir_rsp_t;

/**
 * @struct tz_file_testdir_req_s
 * @brief Command structure for file testdir
 */
typedef struct tz_file_testdir_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pathname of directory */
} __attribute__((packed)) tz_file_testdir_req_t;

/**
 * @struct tz_file_testdir_rsp_s
 * @brief Response structure for file testdir
 */
typedef struct tz_file_testdir_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_testdir_rsp_t;

/**
 * @struct tz_file_telldir_req_s
 * @brief Command structure for file telldir
 */
typedef struct tz_file_telldir_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pathname of directory */
} __attribute__((packed)) tz_file_telldir_req_t;

/**
 * @struct tz_file_telldir_rsp_s
 * @brief Response structure for file telldir
 */
typedef struct tz_file_telldir_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int32_t ret; /**< Current location of directory stream */
} __attribute__((packed)) tz_file_telldir_rsp_t;

/**
 * @struct tz_file_remove_req_s
 * @brief Command structure for file remove
 */
typedef struct tz_file_remove_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pathname of directory */
} __attribute__((packed)) tz_file_remove_req_t;

/**
 * @struct tz_file_remove_rsp_s
 * @brief Response structure for file remove
 */
typedef struct tz_file_remove_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_remove_rsp_t;

/**
 * @struct tz_file_chown_chmod_req_s
 * @brief Command structure for file chown and chmod
 */
typedef struct tz_file_chown_chmod_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char path[TZ_CM_MAX_NAME_LEN]; /**< Memory for path */
	uint32_t path_len; /**< Length of path */
	char word[TZ_CM_MAX_NAME_LEN]; /**< Memory for word */
	uint32_t word_len; /**< Length of word */
	char owner[TZ_CM_MAX_NAME_LEN]; /**< Memory for owner */
	uint32_t owner_len; /**< Length of owner */
	char mod[TZ_CM_MAX_NAME_LEN]; /**< Memory for mode (e.g., 777) */
	uint32_t mod_len; /**< Length of mode */
	uint32_t level; /**< Level */
} __attribute__((packed)) tz_file_chown_chmod_req_t;

/**
 * @struct tz_file_chown_chmod_rsp_s
 * @brief Response structure for file chown and chmod
 */
typedef struct tz_file_chown_chmod_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< 0 for Success / -1 for failure */
} __attribute__((packed)) tz_file_chown_chmod_rsp_t;

/**
 * @struct tz_file_end_req_s
 * @brief Command structure for file end
 */
typedef struct tz_file_end_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
} __attribute__((packed)) tz_file_end_req_t;

/**
 * @struct tz_file_end_rsp_s
 * @brief Response structure for file end
 */
typedef struct tz_file_end_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_end_rsp_t;

/**
 * @struct tz_file_sync_req_s
 * @brief Command structure for file sync
 */
typedef struct tz_file_sync_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int fd; /**< File descriptor */
} __attribute__((packed)) tz_file_sync_req_t;

/**
 * @struct tz_file_sync_rsp_s
 * @brief Response structure for file sync
 */
typedef struct tz_file_sync_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_sync_rsp_t;

/**
 * @struct tz_file_rename_req_s
 * @brief Command structure for file rename
 */
typedef struct tz_file_rename_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	char oldfilename[TZ_CM_MAX_NAME_LEN]; /**< File name to be changed from */
	char newfilename[TZ_CM_MAX_NAME_LEN]; /**< File name to be changed to */
} __attribute__((packed)) tz_file_rename_req_t;

/**
 * @struct tz_file_rename_rsp_s
 * @brief Response structure for file rename
 */
typedef struct tz_file_rename_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_rename_rsp_t;

/**
 * @struct tz_file_err_rsp_s
 * @brief Generic error response structure
 */
typedef struct tz_file_err_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_err_rsp_t;

/**
 * @struct tz_file_par_free_size_req_s
 * @brief Command structure for getting partition free size
 */
typedef struct tz_file_par_free_size_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	uint8_t partition[TZ_CM_MAX_NAME_LEN]; /**< Partition name */
} __attribute__((packed)) tz_file_par_free_size_req_t;

/**
 * @struct tz_file_par_free_size_rsp_s
 * @brief Response structure for getting partition free size
 */
typedef struct tz_file_par_free_size_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	uint64_t size; /**< Free size in bytes */
	int ret; /**< Success/failure value */
} __attribute__((packed)) tz_file_par_free_size_rsp_t;

/**
 * @struct tz_dir_open_req_s
 * @brief Command structure for opening directory stream
 */
typedef struct tz_dir_open_req_s
{
    tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
    char pathname[TZ_CM_MAX_NAME_LEN]; /**< Pointer to directory name with complete path */
} __attribute__ ((packed)) tz_dir_open_req_t;

/**
 * @struct tz_dir_open_rsp_s
 * @brief Response structure for opening directory stream
 */
typedef struct tz_dir_open_rsp_s
{
    tz_fs_msg_cmd_type cmd_id; /**<  First 4 bytes are always command id */
    uint64_t pdir; /**< Pointer to directory stream */
    int ret; /**< Success/failure value */
} __attribute__ ((packed)) tz_dir_open_rsp_t;

/**
 * @struct tz_dir_read_req_s
 * @brief Command structure for reading a directory stream
 */
typedef struct tz_dir_read_req_s
{
    tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
    uint64_t pdir; /**< Pointer to directory stream */
} __attribute__ ((packed)) tz_dir_read_req_t;

/**
 * @struct tz_dir_read_rsp_s
 * @brief Response structure for reading directory stream
 */
typedef struct tz_dir_read_rsp_s
{
    tz_fs_msg_cmd_type cmd_id; /**<  First 4 bytes are always command id */
    struct tz_dirent pdirent; /**<  Directory entry struct */
    int ret; /**<  Success/failure value */
} __attribute__ ((packed)) tz_dir_read_rsp_t;

/**
 * @struct tz_dir_close_req_s
 * @brief Command structure for closing a directory stream
 */
typedef struct tz_dir_close_req_s
{
    tz_fs_msg_cmd_type cmd_id; /**<  First 4 bytes are always command id */
    uint64_t pdir; /**<  Pointer to directory stream */
} __attribute__ ((packed)) tz_dir_close_req_t;

/**
 * @struct tz_dir_close_rsp_s
 * @brief Response structure for closing directory stream
 */
typedef struct tz_dir_close_rsp_s
{
    /** First 4 bytes are always command id */
    tz_fs_msg_cmd_type      cmd_id;
    /** Success/failure value */
    int                     ret;
} __attribute__ ((packed)) tz_dir_close_rsp_t;

/**
 * @struct tz_file_get_errno_req_s
 * @brief Command structure for getting errno
 */
typedef struct tz_file_get_errno_req_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
} __attribute__((packed)) tz_file_get_errno_req_t;

/**
 * @struct tz_file_get_errno_rsp_s
 * @brief Response structure for getting errno
 */
typedef struct tz_file_get_errno_rsp_s {
	tz_fs_msg_cmd_type cmd_id; /**< First 4 bytes are always command id */
	int ret; /**< Error number (0 is success) */
} __attribute__((packed)) tz_file_get_errno_rsp_t;

#endif //__FS_MSG_H_
