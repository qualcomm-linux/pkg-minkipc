// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "backtrace.h"
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include "TUtils.h"

// #define BACKTRACE_VIA_UNWIND

/** Backtrace using unwind library, currently disabled **/
#if defined(BACKTRACE_VIA_UNWIND)
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

/** Macros to control Backtrace details obtained **/
#define BACKTRACE_DEPTH 10
#define BACKTRACE_BUFFER_SIZE 1024
#define MAX_BT_DEPTH 32
#define REG_DUMP 1

/** Signal array for which backtrace is capture.
    More signals can be added to this array seemlessly.
    SIGKILL/SIGSTOP can't handled for backtrace **/
static int32_t gSignumarray[] = {
    SIGSEGV, SIGBUS, SIGILL, SIGABRT, SIGFPE, SIGSYS,
};

#if defined(BACKTRACE_VIA_UNWIND)
/** Backtrace Captured via Library Unwind **/
static void _backtrace_handler(int32_t signum, siginfo_t *info, void *context)
{
    unw_cursor_t cursor;
    unw_word_t backtrace_ip[BACKTRACE_DEPTH];
    unw_word_t ip, sp, reg;
    int32_t bt_level = 0, size = 0, offset = 0, ret, i;
    char backtrace_buffer[BACKTRACE_BUFFER_SIZE];

#if defined(REG_DUMP)
/** X86_64 register dump on host and ARM-64 register dump on target */
#if defined(OFFTARGET) /** Flag identified for cross compilation - TBD */
    unw_regnum_t regdumparr[] = {
        UNW_X86_64_RAX, UNW_X86_64_RDX, UNW_X86_64_RCX, UNW_X86_64_RBX,
        UNW_X86_64_RSI, UNW_X86_64_RDI, UNW_X86_64_RBP, UNW_X86_64_RSP,
        UNW_X86_64_R8,  UNW_X86_64_R9,  UNW_X86_64_R10, UNW_X86_64_R11,
        UNW_X86_64_R12, UNW_X86_64_R13, UNW_X86_64_R14, UNW_X86_64_R15,
    };
#else
    unw_regnum_t regdumparr[] = {
        UNW_ARM_R0, UNW_ARM_R1, UNW_ARM_R2, UNW_ARM_R3,  UNW_ARM_R4,  UNW_ARM_R5,  UNW_ARM_R6,
        UNW_ARM_R7, UNW_ARM_R8, UNW_ARM_R9, UNW_ARM_R10, UNW_ARM_R11, UNW_ARM_R12,
    };
#endif
#endif

    LOG_ERR("\nNew Backtrace:Handler Received %s Signal", strsignal(signum));
    LOG_ERR("Backtrace:PID:%d  TID:%d", getpid(), syscall(__NR_gettid));
    if (unw_init_local(&cursor, context) != 0) {
        LOG_ERR("UnWind UNWIND=== Init Context failed");
        return;
    }

    LOG_ERR(
        "\n=====================Printing Backtrace information via "
        "unwind===================================");
    do {
        char func_name[128];
        if (bt_level >= BACKTRACE_DEPTH) break;

        if (unw_get_reg(&cursor, UNW_REG_IP, &ip) == 0) {
            unw_get_reg(&cursor, UNW_REG_SP, &sp);
            backtrace_ip[bt_level] = ip;
            if ((ret = unw_get_proc_name(&cursor, func_name, sizeof(func_name),
                                         (unw_word_t *)&offset)) == 0) {
                size = snprintf(backtrace_buffer, sizeof(backtrace_buffer),
                                "%2d:0x%09lx : %s()+0x%lx SP:0x%lx\n", bt_level, (uintptr_t)ip,
                                func_name, offset, sp);
            } else {
                if (ret == UNW_ENOINFO) LOG_ERR("Function name not known");
                if (ret == UNW_ENOMEM) LOG_ERR("No memory");
                if (ret == UNW_EUNSPEC) LOG_ERR("UNSPECIFIED ERROR");
                size = snprintf(backtrace_buffer, sizeof(backtrace_buffer),
                                "%2d:0x%09lx : (Unknown)+0x%lx SP:0x%lx\n", bt_level, (uintptr_t)ip,
                                offset, sp);
            }

            backtrace_buffer[size] = '\0';
            bt_level++;
            LOG_ERR("%s", backtrace_buffer);
        }
    } while (unw_step(&cursor) > 0);

    LOG_ERR("Register Dump");
    for (i = 0; i < sizeof(regdumparr) / sizeof(regdumparr[0]); i++) {
        unw_get_reg(&cursor, regdumparr[i], &reg);
        LOG_ERR("Register %s:0x%llx", unw_regname(regdumparr[i]), reg);
    }

    LOG_ERR(
        "\n=====================End of Backtrace handler=========================================");
    signal(signum, SIG_DFL);
}
#else
static void _backtrace_handler(int32_t signum, siginfo_t *info, void *pcontext)
{
    int32_t bt_depth, i;
    void *bt_addr[MAX_BT_DEPTH];
    ucontext_t *context = (ucontext_t *)pcontext;
    /** reset the signal handler to the default action SIG_DFL when
     * _backtrace_handler is done */
    struct sigaction restore_action;
    sigemptyset(&restore_action.sa_mask);
    restore_action.sa_handler = SIG_DFL;
    restore_action.sa_flags = 0;

    LOG_ERR("New Backtrace:Handler Received %s Signal", strsignal(signum));
    LOG_ERR("Backtrace:PID:%d  TID:%ld", getpid(), syscall(__NR_gettid));

    bt_depth = backtrace(bt_addr, MAX_BT_DEPTH);
    LOG_ERR("backtrace() returned %d addresses", bt_depth);

    LOG_ERR(
        "=====================Printing Backtrace "
        "information===================================");
    backtrace_symbols_fd(bt_addr, bt_depth, STDERR_FILENO);
    LOG_ERR(
        "=====================End of Backtrace "
        "handler=========================================");

#if defined(REG_DUMP)
/** X86_64 register dump on host and ARM-64 register dump on target */
#if defined(OFFTARGET) /** Flag identified for cross compilation - TBD */
    for (i = 0; i < NGREG; i++) {
        LOG_ERR("Register %d:  0x%llx", i, context->uc_mcontext.gregs[i]);
    }
#else                  // ARM 64 bit
    for (i = 0; i < 31; i++) {
        LOG_ERR("Register %d:  0x%llx", i, context->uc_mcontext.regs[i]);
    }

    LOG_ERR("Fault Address %d:  0x%llx", i, context->uc_mcontext.fault_address);
    LOG_ERR("PSTATE %d:  0x%llx", i, context->uc_mcontext.pstate);
#endif
#endif
    sigaction(signum, &restore_action, NULL);
}
#endif

/** Function invoked from premain which is part of libmodule
 * This function has to be invoked in each process which needs backtrace
 */
int32_t register_backtrace_handlers(uint32_t signals)
{
    int32_t i;
    struct sigaction sigobject = {0};

    /** Register the backtrace handler for capturing backtraces */
    sigemptyset(&sigobject.sa_mask);
    sigobject.sa_sigaction = &_backtrace_handler;
    sigobject.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    /** Based on argument register signal for ALL signals or for a particular
     * signal **/
    if (signals == BACKTRACE_ALL_SIGNALS) {
        LOG_MSG("New Backtrace: Registering signal Handlers for ALL Signals:%lu",
                sizeof(gSignumarray) / sizeof(gSignumarray[0]));
        for (i = 0; i < sizeof(gSignumarray) / sizeof(gSignumarray[0]); i++) {
            if (sigaction(gSignumarray[i], &sigobject, NULL)) {
                LOG_ERR("Backtrace: Registration failed for signal:%d", gSignumarray[i]);
            }
        }

        LOG_MSG("New Registration successful for ALL signals");
    } else {
        LOG_MSG("Backtrace: Registering signal Handlers for Signal%d", signals);
        if (sigaction(signals, &sigobject, NULL)) {
            LOG_ERR("Backtrace: Registration failed for signal:%d", signals);
        } else {
            LOG_MSG("Registration successful for specified signals");
        }
    }

    return 0;
}
