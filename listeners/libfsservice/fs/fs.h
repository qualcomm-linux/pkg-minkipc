// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __FS_H_
#define __FS_H_

/**
 * @brief Opens a file as requested by the QTEE file open request.
 *
 * This function processes a file open request from QTEE, optionally preopens
 * the file if O_CREAT is specified, and sets the result in the response
 * structure.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return int File descriptor on success, -1 on failure.
 */
int file_open(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Opens a file relative to a directory file descriptor.
 *
 * This function processes a file open request relative to a directory file
 * descriptor using `openat()` system call. The result is stored in the
 * response structure.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_openat(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Unlinks (removes) a file relative to a directory file descriptor.
 *
 * This function processes a unlink request from QTEE by resolving the
 * vendor-prefixed path if needed, and removing the file using `unlinkat()`.
 * The result is stored in the response structure.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_unlinkat(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Creates a new file with the specified mode.
 *
 * This function processes a file create request from QTEE, resolves the
 * vendor-prefixed path if needed, and creates the file using `creat()`.
 * The result is stored in the response structure.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_creat(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Reads data from a file descriptor into a buffer.
 *
 * This function processes a file read request from QTEE and reads up to
 * `count` bytes from the file descriptor into the response buffer. If
 * `count` exceeds the maximum allowed buffer size, it is capped.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_read(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Writes data to a file descriptor from a buffer.
 *
 * This function processes a file write request from QTEE and writes up to
 * `count` bytes from the request buffer to the file descriptor. If `count`
 * exceeds the maximum allowed buffer size, it is capped.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_write(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Retrieves file status information for a given file descriptor.
 *
 * This function processes a file status request from QTEE and fills the
 * tz_file_fstat_rsp_t response with file metadata using `fstat()`.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_fstat(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Retrieves file status information for a given path using lstat().
 *
 * This function processes a file status request from QTEE and fills the
 * tz_file_lstat_rsp_t response with file metadata using `lstat()`.
 * It resolves the vendor-prefixed path if needed.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_lstat(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Creates a new link (hard link) to an existing file.
 *
 * This function processes a file link request, resolves the
 * vendor-prefixed paths for both the old and new file paths, and creates
 * a hard link using `link()`. The result is stored in the response structure.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_link(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Removes a file specified by its pathname.
 *
 * This function processes a file unlink request from QTEE, resolves the
 * vendor-prefixed path if needed, and removes the file using `unlink()`.
 * The result is stored in the response structure.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_unlink(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Closes a file descriptor.
 *
 * This function processes a file close request from QTEE and closes the
 * file descriptor using `close()`. The result is stored in the response
 * structure.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_close(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Removes a file or directory specified by the pathname.
 *
 * This function processes a file/directory remove request from QTEE,
 * resolves the vendor-prefixed path if needed, and removes the file using
 * `unlink()` or the directory using `rmdir_h()`. The result is stored in
 * the response structure.
 *
 * @param req Pointer to the request buffer.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response buffer.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_remove(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Rename a file from old name to new name in the vendor path.
 *
 * This function processes a file rename operation from QTEE using the vendor
 * path resolution.
 *
 * @param req Pointer to the request structure.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response structure.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_rename(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Set a default error response for file operations.
 *
 * This function sets the response structure with a default error code
 * indicating that no valid command was received.
 *
 * @param rsp Pointer to the response structure.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0.
 */
int file_error(void *rsp, size_t rsp_len);

/**
 * @brief Set file partition not available error response.
 *
 * Sets error_number to EAGAIN to indicate to QTEE that the file partition is not
 * yet mounted and it should try again later.
 * Sets the response structure's return value to -1 and returns the size of the
 * structure.
 *
 * @param id  Command ID indicating the type of file system operation.
 * @param rsp Pointer to the response structure.
 * @return Size of the response structure set.
 */
size_t file_partition_error(uint32_t id, void *rsp);

/**
 * @brief Process a file control request from QTEE using fcntl.
 *
 * Performs the specified fcntl operation on the given file descriptor.
 *
 * @param req Pointer to the request structure.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response structure.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_fcntl(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Process a file seek request from QTEE using lseek.
 *
 * Performs a seek operation on the given file descriptor.
 *
 * @param req Pointer to the request structure.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response structure.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_lseek(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Process a directory remove request from QTEE.
 *
 * Attempts to remove the directory specified by QTEE.
 *
 * @param req Pointer to the request structure.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response structure.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_rmdir(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Process a directory create request from QTEE.
 *
 * Attempts to create the directory.
 *
 * @param req Pointer to the request structure.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response structure.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. On failure, sets ret to -1 and error_number.
 */
int file_mkdir(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Tests if a directory exists at the given path.
 *
 * @param req Pointer to request buffer.
 * @param req_len Length of request buffer.
 * @param rsp Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return Always returns 0. Result is stored in rsp->ret.
 */
int file_testdir(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Handle unsupported telldir request from QTEE.
 *
 * Always returns -1 as telldir is not supported.
 *
 * @param req Pointer to the request structure (unused).
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response structure.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0 and sets response return to -1.
 */
int file_telldir(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Handle end-of-file request from QTEE.
 *
 * Returns 0 to indicate successful completion of the end operation.
 *
 * @param req Pointer to the request structure (unused).
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response structure.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0.
 */
int file_end(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Process partition free size request from QTEE.
 *
 * Uses the statfs() system call to figure out the free space available
 * in a partition.
 *
 * @param req Pointer to request buffer.
 * @param req_len Length of request buffer.
 * @param rsp Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return Always returns 0. Error returned to QTEE in rsp->ret.
 */
int file_get_partition_free_size(void *req, size_t req_len, void *rsp,
				 size_t rsp_len);

/**
 * @brief Changes ownership and permissions of a directory and its subfolders.
 *
 * @param req Pointer to request buffer.
 * @param req_len Length of request buffer.
 * @param rsp Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return Always returns 0. Result is stored in rsp->ret.
 */
int file_dir_chown_chmod(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Synchronize a file's in-memory state with storage.
 *
 * Performs an fsync operation on the specified file descriptor.
 *
 * @param req Pointer to the request structure.
 * @param req_len Length of the request buffer.
 * @param rsp Pointer to the response structure.
 * @param rsp_len Length of the response buffer.
 * @return Always returns 0. The result is stored in the response.
 */
int file_sync(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Process a directory stream open request from QTEE.
 *
 * @param req     Pointer to request buffer.
 * @param req_len Length of request buffer.
 * @param rsp     Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return        Always returns 0. Error returned to QTEE in rsp->ret.
 */
int dir_open(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Process a directory entry read request from QTEE.
 *
 * @param req     Pointer to request buffer.
 * @param req_len Length of request buffer.
 * @param rsp     Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return        Always returns 0. Error returned to QTEE in rsp->ret.
 */
int dir_read(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Process a directory stream close request from QTEE.
 *
 * @param req     Pointer to request buffer.
 * @param req_len Length of request buffer.
 * @param rsp     Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return        Always returns 0. Error returned to QTEE in rsp->ret.
 */
int dir_close(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Return the last recorded file system error to QTEE.
 *
 * Retrieves the global error number and stores it in the response.
 *
 * @param rsp Pointer to the response structure.
 * @param rsp_len Length of response buffer.
 * @return Always returns 0. The result is stored in the response.
 */
int file_get_errno(void *rsp, size_t rsp_len);

#endif // __FS_H_
