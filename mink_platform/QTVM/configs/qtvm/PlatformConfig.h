// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

// TVM hosts all of the services and none of the TAs.
#ifndef __PLATFORM_CONFIG_H
#define __PLATFORM_CONFIG_H

#include "CQTVMPlatformInfo.h"
#include "CTAccessControl.h"
#include "CTMemoryService.h"
#include "CTPowerService.h"
#include "CTProcessLoader.h"
#include "CTRebootVM.h"
#include "MinkHub.h"
#include "minksocket.h"
#include "vmuuid.h"

// enables
#define ENABLE_QRTR_SERVER true

// VSOCK configurations
#define ENABLE_VSOCK_SERVER true
#define OPEN_VSOCK_SERVICE false

// QMSGQ configurations
#define ENABLE_QMSGQ_SERVER false
#define OPEN_QMSGQ_SERVICE false

#define ROOT_SOCKET_ENV_VAR "LE_SOCKET_root"
#define PRELAUNCHER_SOCKET_ENV_VAR "LE_SOCKET_prelauncher"
#define MINKD_SOCKET_ENV_VAR "LE_SOCKET_minkd"

#define MINK_QRTR_SERVICE_FROM_LA "5009"
#define MINK_AVM_SOCKET_NAME MINK_QRTR_SERVICE_FROM_LA

#define MINK_HLOS_SOCKET_NAME "5033"

// Daemon Name
#define PRELAUNCHER_DAEMON_NAME "TVMPrelauncher"
#define MINK_DAEMON_NAME "TVMMink"

#define PROCESS_LOADER_UID CTProcessLoader_UID
#define POWER_SERVICE_UID CTPowerService_UID
#define MEMPOOL_FACTORY_UID CTMemPoolFactory_UID
#define PLATFORM_INFO_SERVICE_UID CQTVMPlatformInfo_UID
#define REBOOT_SERVICE_UID CTRebootVM_UID
#define TACCESS_CONTROL_UID CTAccessControl_UID

#define LOCAL_OSID "qtvm"

#ifndef OFFTARGET
// On Device

// enables
#define ENABLE_TUNNEL_INVOKE false

// definitions
#define MINK_QRTR_SERVICE_TO_LA MINK_QRTR_SERVICE_FROM_TVM
#define MINK_QRTR_SERVICE_TO_LA_VERSION MINK_QRTR_SERVICE_FROM_TVM_VERSION
#define MINK_QRTR_SERVICE_TO_LA_INSTANCE MINK_QRTR_SERVICE_FROM_TVM_INSTANCE

#define MINK_QRTR_SERVICE_FROM_TVM 5008
#define MINK_QRTR_SERVICE_FROM_TVM_VERSION 1
#define MINK_QRTR_SERVICE_FROM_TVM_INSTANCE 1

#define MINK_VSOCK_SERVICE_TO_XVM MINK_VSOCK_OEM_PORT_FROM_TVM
#define MINK_VSOCK_OEM_PORT_FROM_TVM 0x6666

#define MINK_QMSGQ_SERVICE_TO_XVM MINK_QMSGQ_OEM_PORT_FROM_TVM
#define MINK_QMSGQ_OEM_PORT_FROM_TVM 0x6543

/* VSOCK */
#define MINK_VSOCK_SERVICE_FROM_XVM MINK_VSOCK_OEM_PORT_FROM_OEMVM
#define MINK_VSOCK_OEM_PORT_FROM_OEMVM "26215"  // (0x6667)

#define MINK_XVM_VSOCK_NAME MINK_VSOCK_SERVICE_FROM_XVM
#define MINK_XVM_VSOCK_TYPE MINKHUB_VSOCK

/* QMSGQ */
#define MINK_QMSGQ_SERVICE_FROM_XVM MINK_QMSGQ_OEM_PORT_FROM_OEMVM
#define MINK_QMSGQ_OEM_PORT_FROM_OEMVM "25924"  // (0x6544)

#define MINK_XVM_QMSGQ_NAME MINK_QMSGQ_SERVICE_FROM_XVM
#define MINK_XVM_QMSGQ_TYPE MINKHUB_QMSGQ

#define MINK_AVM_SOCKET_TYPE MINKHUB_QIPCRTR

// Unix sockets for local connections
#define SOCKET_DIR_NAME "/dev/socket/"
#define ROOT_SOCKET_FILE_NAME "root"
#define PRELAUNCHER_SOCKET_FILE_NAME "prelauncher"
#define MINKD_SOCKET_FILE_NAME "minkd"

// Configure file Dir
#define CONFIGURE_DIR "/etc/profiles"

#else
// OFFTARGET
// enables
#define ENABLE_TUNNEL_INVOKE false

// Sockets to other VM
#define MINK_VSOCK_SIMULATED_FD_ENV_VAR "not_applicable"
#define MINK_VSOCK_SIMULATED_SOCKET_NAME "tvmd_vsock_simulated_socket"
#define MINK_QRTR_SIMULATED_FD_ENV_VAR "not_applicable"
#define MINK_QRTR_SIMULATED_SOCKET_NAME "tvm_qrtr_simulated_socket"
#define MINK_QMSGQ_SIMULATED_FD_ENV_VAR "not_applicable"
#define MINK_QMSGQ_SIMULATED_SOCKET_NAME "tvmd_qmsgq_simulated_socket"

// Sockets from other VM
#define MINK_VSOCK_SERVICE_FROM_XVM_SIMULATED "oemvm_vsock_simulated_socket"
#define MINK_XVM_VSOCK_NAME MINK_VSOCK_SERVICE_FROM_XVM_SIMULATED
#define MINK_XVM_VSOCK_TYPE MINKHUB_SIMULATE

#define MINK_QMSGQ_SERVICE_FROM_XVM_SIMULATED "oemvm_qmsgq_simulated_socket"
#define MINK_XVM_QMSGQ_NAME MINK_QMSGQ_SERVICE_FROM_XVM_SIMULATED
#define MINK_XVM_QMSGQ_TYPE MINKHUB_SIMULATE

#define MINK_AVM_SOCKET_TYPE MINKHUB_UNIX

// Unix sockets for local connections
#define SOCKET_DIR_NAME "./"
#define ROOT_SOCKET_FILE_NAME "tvm_root_socket"
#define PRELAUNCHER_SOCKET_FILE_NAME "tvm_prelauncher_socket"
#define MINKD_SOCKET_FILE_NAME "tvm_minkd_socket"

// Configure file Dir
#define CONFIGURE_DIR "profiles/off-target/qtvm"

#endif

#define ROOT_SOCKET_NAME SOCKET_DIR_NAME ROOT_SOCKET_FILE_NAME
#define PRELAUNCHER_SOCKET_NAME SOCKET_DIR_NAME PRELAUNCHER_SOCKET_FILE_NAME
#define MINKD_SOCKET_NAME SOCKET_DIR_NAME MINKD_SOCKET_FILE_NAME

#define VM_UUID CLIENT_VMUID_TUI

#endif  // __PLATFORM_CONFIG_H
