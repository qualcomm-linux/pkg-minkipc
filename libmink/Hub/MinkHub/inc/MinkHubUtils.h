// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef _MINKHUB_UTILS_H_
#define _MINKHUB_UTILS_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include "object.h"

#if defined(OFFTARGET)
/* Define method to print thread ID */
#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif
#define gettid() ((pid_t)syscall(SYS_gettid))
#else
#define gettid() ((pid_t)syscall(__NR_gettid))
#endif

/******************************************************************************/
/**********************  Global Function Definitions  *************************/
/******************************************************************************/

static inline int32_t atomicAdd(int32_t *pn, int32_t n)
{
    return __sync_add_and_fetch(pn, n);  // GCC builtin
}

/******************************************************************************/
/************************  Helper Functions/Macros  ***************************/
/******************************************************************************/

/**
 * Return a pointer to a structure of the specified type, given a pointer to
 * and name of one of its members.
 */
#define C_CONTAINEROF(ptr, type, member) \
    ((type *)(((uintptr_t)(void *)ptr) - offsetof(type, member)))

/**
 * Return the *length* of an array, which is the number of *elements* it
 * contains.  This is not to be confused with the array's *size*, as
 * returned by sizeof, which is the number of *bytes* it occupies in memory.
 *
 * We aim to mitigate situations in which a pointer is accidentally
 * supplied instead of an array by evaluating to zero in those cases.
 * "a == &a" is always true for arrays, and not generally for pointers.
 */
#define C_LENGTHOF(array) \
    ((void *)&(array) == (void *)(array) ? sizeof(array) / sizeof *(array) : 0)

void MINKHUB_printLog(const char *fmt, ...);
void MINKHUB_printLogWithTail(uint32_t pid, uint32_t tid, const char *fulFunc, bool addFail,
                              int32_t rv);

/* More complicated stuff */
#define _NUM_ARGS_EVAL_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, N, ...) N

#define _NUM_ARGS_(...) \
    _NUM_ARGS_EVAL_(_0, ##__VA_ARGS__, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define _NUM_ARGS_MIN_2_(...) \
    _NUM_ARGS_EVAL_(_0, ##__VA_ARGS__, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0)

#define _NUM_ARGS_MIN_4_(...) \
    _NUM_ARGS_EVAL_(_0, ##__VA_ARGS__, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0)

#define _GET_MACRO_(name, n) name##n

#define _GET_MACRO(name, n) _GET_MACRO_(name, n)

#define GET_MACRO(macro, ...) _GET_MACRO(macro, _NUM_ARGS_(__VA_ARGS__))(__VA_ARGS__)

/* optional arguments */
#define GET_MACRO_MIN_2(macro, ...) _GET_MACRO(macro, _NUM_ARGS_MIN_2_(__VA_ARGS__))(__VA_ARGS__)

#define GET_MACRO_MIN_4(macro, ...) _GET_MACRO(macro, _NUM_ARGS_MIN_4_(__VA_ARGS__))(__VA_ARGS__)

#define MINKHUB_LOG(...) GET_MACRO_MIN_2(MINKHUB_LOG, __VA_ARGS__)
#define MINKHUB_LOG2(a, b) MINKHUB_LOG3(a, b, "")

#define MINKHUB_LOG3(fullFuncDef, failFmt, tailFmt, ...)                     \
    {                                                                        \
        MINKHUB_printLogWithTail(getpid(), gettid(), fullFuncDef, false, 0); \
    }

#define MINKHUB_CHECK_FUNC(...) GET_MACRO_MIN_4(MINKHUB_CHECK_FUNC, __VA_ARGS__)
#define MINKHUB_CHECK_FUNC4(a, b, c, d) MINKHUB_CHECK_FUNC5(a, b, c, d, "")

/* Call local functions to format a failure message, check if the passing
 * criteria is true and return an error if it is not. */
#define MINKHUB_CHECK_FUNC5(rv, errorCode, fullFuncDef, expectTrue, tailFmt, ...) \
    {                                                                             \
        if (!(expectTrue)) {                                                      \
            rv = (errorCode);                                                     \
            MINKHUB_printLogWithTail(getpid(), gettid(), fullFuncDef, true, rv);  \
            goto exit;                                                            \
        }                                                                         \
    }

/******************************************************************************/
/************************  Public Macro Definitions  **************************/
/******************************************************************************/

/* Print a message to the terminal */
#if defined(DEBUG)
#define MINKHUB_LOG_MSG(xx_fmt, ...)                                                          \
    {                                                                                         \
        MINKHUB_printLog("(%5u:%-5u) Info  %s:%u " xx_fmt "\n", getpid(), gettid(), __func__, \
                         __LINE__, ##__VA_ARGS__);                                            \
    }
#else
#define MINKHUB_LOG_MSG(xx_fmt, ...) \
    do {                             \
    } while (0);
#endif

#define MINKHUB_LOG_ERR(xx_fmt, ...)                                                          \
    {                                                                                         \
        MINKHUB_printLog("(%5u:%-5u) Error %s:%u " xx_fmt "\n", getpid(), gettid(), __func__, \
                         __LINE__, ##__VA_ARGS__);                                            \
    }

/* Call an expression which does not save the result (i.e. there is no assign).
 * The purpose of this is to get 'Trace' in the log messages and avoid extra
 * 'LOG_MSG' in the code. It prints all of 'func', not just up to the first
 * '('. */
#define MINKHUB_TRACE(func)                                                                  \
    {                                                                                        \
        MINKHUB_printLog("(%5u:%-5u) Trace %s:%u " #func "\n", getpid(), gettid(), __func__, \
                         __LINE__);                                                          \
        func;                                                                                \
    }

/* MINKHUB_CHECK* and MINKHUB_GUARD* check that a condition is TRUE or that a call returns
 * Object_OK, respectively. With passing criteria, nothing is printed to the
 * terminal and execution continues. For failing criteria, an error is printed
 * and execution will 'goto exit'. */
#define MINKHUB_CHECK_ERR(cond, errorCode)                         \
    if (!(cond)) {                                                 \
        ret = (errorCode);                                         \
        MINKHUB_LOG_ERR("(%s) --> failed : %d", #cond, errorCode); \
        goto exit;                                                 \
    }

#define MINKHUB_CHECK(cond)                   \
    {                                         \
        MINKHUB_CHECK_ERR(cond, Object_ERROR) \
    }

#define MINKHUB_GUARD_ERR(status, errorCode)                      \
    {                                                             \
        MINKHUB_CHECK_ERR(Object_isOK(ret = (status)), errorCode) \
    }

#define MINKHUB_GUARD(status)          \
    {                                  \
        MINKHUB_GUARD_ERR(status, ret) \
    }

/* Log function name and THEN call function. There is never a fail message, but
 * the user may define a format to print on the same line as the Trace. */
#define MINKHUB_CALL_NO_CHECK(rv, func, ...) \
    MINKHUB_LOG(#func, "", ##__VA_ARGS__);   \
    rv = func;

/* Same at MINKHUB_CALL, but user specifies the lhs, pass criteria, and error value */
#define MINKHUB_CALL_CHECK_ERR(lhs, func, expectTrue, errorCode, ...)         \
    {                                                                         \
        MINKHUB_CALL_NO_CHECK(lhs, func);                                     \
        MINKHUB_CHECK_FUNC(ret, errorCode, #func, expectTrue, ##__VA_ARGS__); \
    }

/* Same at MINKHUB_CALL, but user specifies the pass criteria */
#define MINKHUB_CALL_CHECK(func, expectTrue, ...)                          \
    {                                                                      \
        MINKHUB_CALL_CHECK_ERR(ret, func, expectTrue, ret, ##__VA_ARGS__); \
    }

/* Same at MINKHUB_CALL, but user can over-ride the error value */
#define MINKHUB_CALL_ERR(func, errorCode, ...)                                         \
    {                                                                                  \
        MINKHUB_CALL_CHECK_ERR(ret, func, Object_isOK(ret), errorCode, ##__VA_ARGS__); \
    }

/* Print expression to terminal, call expression and then check that error value
 * is OK, else print an error and go to exit */
#define MINKHUB_CALL(func, ...)                     \
    {                                               \
        MINKHUB_CALL_ERR(func, ret, ##__VA_ARGS__); \
    }

#define MINKHUB_LOG_RESULT(prefix, service, ret)                                               \
    MINKHUB_LOG_MSG("%s to " prefix "(%x). ret = %d\n", Object_isOK(ret) ? "Succeed" : "Fail", \
                    (service), (ret))

#endif  // _MINKHUB_UTILS_H_
