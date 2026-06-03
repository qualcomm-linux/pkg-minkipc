// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef ERROR_MAP_H
#define ERROR_MAP_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int32_t errorFrom; /**< error code from the input error space */
    int32_t errorTo;   /**< error code from the output error space */
} ErrorPair;

/**
 * The error space map is constituted of an array of corresponding
 * input/output errors, the length of the array and the generic error code of the output space.
 * It is recommended, for performance reasons, that the OK/SUCCESS return values
 * be placed in the first position of the array.
 * The structure is optimized to ease conversion among different Mink IDL
 * interfaces, for which startConversionAt is to be set to Object_ERROR_USERBASE.
 *
 * To enable conversion on all errors (e.g. converting to qsee_errno.h values),
 * set startConversionAt=INT32_MIN.
 * */
typedef struct {
    ErrorPair const* errors;
    size_t length;
    int32_t genericError;      /**< generic error from the output error space */
    int32_t startConversionAt; /**< error smaller than this value will be allowed to pass through */
} ErrorMap;

/** Convert an error from the input to the output error space.
 * Returns the generic error of the output error space in case of no match. */
int32_t ErrorMap_convert(ErrorMap const* me, int32_t error);

/** Convert an error from the input to the output error space.
 * Returns the passed default generic error in case of no match.
 * */
int32_t ErrorMap_convertWithDefault(ErrorMap const* me, int32_t error, int32_t generic);

#endif  // ERROR_MAP_H
