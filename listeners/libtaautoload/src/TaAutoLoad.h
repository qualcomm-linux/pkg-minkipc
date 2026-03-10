// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <stdlib.h>
#include <object.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and register for automatic TA loading.
 *
 * @return Returns 0 on success.
 *         Returns -1 on failure.
 */
int register_service();

/**
 * @brief De-Initialize and de-register automatic TA loading.
 */
void deregister_service();


#ifdef __cplusplus
}
#endif
