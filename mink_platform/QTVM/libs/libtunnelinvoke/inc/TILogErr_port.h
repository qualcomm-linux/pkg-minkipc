// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TILOGERR_PORT_H__
#define __TILOGERR_PORT_H__

#include <stdint.h>
#include <stdio.h>

#define TI_LOG_ERR_PORT(...) GET_MACRO_TI(TI_LOG_ERR_, ##__VA_ARGS__)

/* Helper macros: */
#define _NUM_ARGS_EVALUATOR_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, \
                             _16, _17, _18, _19, _20, N, ...)                                  \
    N

#define _NUM_ARGS_COUNT_(...)                                                                      \
    _NUM_ARGS_EVALUATOR_(_0, ##__VA_ARGS__, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, \
                         4, 3, 2, 1, 0)

#define _GET_MACRO_TI_(name, n) name##n

#define _GET_MACRO_TI(name, n) _GET_MACRO_TI_(name, n)

#define GET_MACRO_TI(macro, ...) _GET_MACRO_TI(macro, _NUM_ARGS_COUNT_(__VA_ARGS__))(__VA_ARGS__)

#define TI_LOG_ERR_1(errCode) printf("TI Error! Err code: %d \n", (unsigned int)errCode);

#define TI_LOG_ERR_2(errCode, p1) \
    printf("TI Error! Err code: %d Params: %lx \n", (unsigned int)errCode, (unsigned long)p1);

#define TI_LOG_ERR_3(errCode, p1, p2)                                                              \
    printf("TI Error! Err code: %d Params: %lx, %lx \n", (unsigned int)errCode, (unsigned long)p1, \
           (unsigned long)p2);

#define TI_LOG_ERR_4(errCode, p1, p2, p3)                                            \
    printf("TI Error! Err code: %d Params: %lx, %lx, %lx \n", (unsigned int)errCode, \
           (unsigned long)p1, (unsigned long)p2, (unsigned long)p3);

#define TI_LOG_ERR_5(errCode, p1, p2, p3, p4)                                             \
    printf("TI Error! Err code: %d Params: %lx, %lx, %lx, %lx \n", (unsigned int)errCode, \
           (unsigned long)p1, (unsigned long)p2, (unsigned long)p3, (unsigned long)p4);

#endif  //__TILOGERR_PORT_H__
