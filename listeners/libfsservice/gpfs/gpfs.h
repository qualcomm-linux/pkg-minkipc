// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __GPFS_H_
#define __GPFS_H_

/**
 * @brief Processes a tz_gpfile_read_req_t request from TZ and sets the
 *        tz_gpfile_read_rsp_t response.
 *
 * Steps:
 * 1. Opens the file specified in the pathname.
 * 2. Seeks to the provided offset.
 * 3. Reads the number of bytes specified by count.
 *    - If count > buffer size, reads only buffer size.
 *    - If count <= buffer size and within file size, returns count.
 * 4. Closes the file.
 *
 * @note On any error, err is set to errno and return value is 0.
 *
 * @param req Pointer to request buffer.
 * @param req_len Length of request buffer.
 * @param rsp Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return Always returns 0.
 */
int gpfile_read(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Processes a tz_gpfile_write_req_t request from TZ and sets the
 *        tz_gpfile_write_rsp_t response.
 *
 * Steps:
 * 1. Opens the file specified in the pathname.
 * 2. If backup is specified, creates a backup of the file.
 * 3. Seeks to the provided offset.
 * 4. Writes the number of bytes from buf specified by count.
 *    - If count > buffer size, writes only buffer size.
 *    - Returns count <= buffer size in response.
 * 5. Closes the file.
 *
 * @note On any error, err is set to errno and return value is 0.
 *
 * @param req Pointer to request buffer.
 * @param req_len Length of request buffer.
 * @param rsp Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return Always returns 0.
 */
int gpfile_write(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Processes a tz_gpfile_remove_req_t request from TZ and sets the
 *        tz_gpfile_remove_rsp_t response.
 *
 * Removes the specified file.
 *
 * @note On error, err is set to errno.
 *
 * @param req Pointer to request buffer.
 * @param req_len Length of request buffer.
 * @param rsp Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return Always returns 0.
 */
int gpfile_remove(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Processes a tz_gpfile_rename_req_t request from TZ and sets the
 *        tz_gpfile_rename_rsp_t response.
 *
 * Renames the specified file.
 *
 * @note On error, err is set to errno.
 *
 * @param req Pointer to request buffer.
 * @param req_len Length of request buffer.
 * @param rsp Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return Always returns 0.
 */
int gpfile_rename(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Sets tz_gpfile_checkversion_rsp_t response to return the SFS version
 *        to TZ.
 *
 * @param req Pointer to request buffer (unused).
 * @param req_len Length of request buffer.
 * @param rsp Pointer to response buffer.
 * @param rsp_len Length of response buffer.
 * @return Always returns 0.
 */
int gpfile_check_version(void *req, size_t req_len, void *rsp, size_t rsp_len);

/**
 * @brief Sets tz_gpfile_error_rsp_t response for unrecognized command IDs.
 *
 * @note Sets err to GPFS_ERROR_NO_CMD.
 *
 * @param rsp Pointer to response buffer.
 * @return Always returns 0.
 */
int gpfile_error(void *rsp, size_t rsp_len);

/**
 * @brief Sets tz_gpfile_checkversion_rsp_t response to indicate that the
 *        persist partition is not mounted.
 *
 * @note Sets err to EAGAIN.
 *
 * @param rsp Pointer to response buffer.
 * @return Always returns 0.
 */
int gpfile_partition_error(void *rsp, size_t rsp_len);


#endif // __GPFS_H_
