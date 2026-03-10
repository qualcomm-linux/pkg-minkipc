// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __FS_HELPER_
#define __FS_HELPER_

#include <stdbool.h>

/**
 * @brief Checks whether the persist partition is mounted.
 *
 * This function scans `/proc/mounts` to determine if the persist partition
 * is mounted. If not found, it logs an error and returns false.
 *
 * @return true if the persist partition is mounted, false otherwise.
 */
bool is_persist_partition_mounted(void);

/**
 * @brief Checks if the directory part of a given path exists.
 *
 * This function extracts the directory portion of the given path and
 * attempts to open it using `opendir()`.
 *
 * @param path The full file path to check.
 * @return 0 if the directory exists, -1 otherwise.
 */
int check_dir_path(const char *path);

/**
 * @brief Determines if the given path is a legacy persist path that
 *        requires prefixing.
 *
 * Compares the path against `LEGACY_PERSIST_PATH` to decide if it needs
 * to be prepended with the persist path.
 *
 * @param path The path to check.
 * @return true if the path is a legacy persist path, false otherwise.
 */
bool is_persist_path_need_append(const char *path);

/**
 * @brief Checks if the given path is part of the whitelist.
 *
 * Iterates through the global whitelist array and compares each entry
 * with the provided path.
 *
 * @param path The path to check.
 * @return true if the path is in the whitelist, false otherwise.
 */
bool is_whitelist_path(const char *path);

/**
 * @brief Returns a resolved and prefixed path if required, otherwise returns the
 *        original path.
 *
 * This function checks if the given path needs to be prefixed with a Data or Persist
 * directory path (e.g., DATA_PATH or PERSIST_PATH) based on its type and
 * permissions.
 *
 * @param old_path The original file path.
 * @param old_len Length of the original path.
 * @param new_path Buffer to store the new path if modified.
 * @param new_len Length of the new path buffer.
 * @return const char* The modified path if applicable, otherwise the original.
 */
char *get_resolved_path(char *old_path, size_t old_len, char *new_path,
			size_t new_len);

#endif // __FS_HELPER_
