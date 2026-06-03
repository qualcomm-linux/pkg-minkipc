// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

// OEMVM hosts all of the TAs and none of the services.
#ifndef __PLATFORM_CONFIG_H
#define __PLATFORM_CONFIG_H

#include "COEMVMPlatformInfo.h"
#include "CTOEMVMAccessControl.h"
#include "CTOEMVMMemoryService.h"
#include "CTOEMVMPowerService.h"
#include "CTOEMVMProcessLoader.h"
#include "CTRebootOEMVM.h"
#include "minksocket.h"
#include "vmuuid.h"

// enables
#define ENABLE_TUNNEL_INVOKE false
#define ENABLE_QRTR_SERVER true

// VSOCK configurations
#define ENABLE_VSOCK_SERVER false
#define OPEN_VSOCK_SERVICE true

// QMSGQ configurations
#define ENABLE_QMSGQ_SERVER false
#define OPEN_QMSGQ_SERVICE false

#define ROOT_SOCKET_ENV_VAR "LE_SOCKET_root"
#define PRELAUNCHER_SOCKET_ENV_VAR "LE_SOCKET_prelauncher"
#define MINKD_SOCKET_ENV_VAR "LE_SOCKET_minkd"

#define MINK_QRTR_SERVICE_FROM_LA "5009"
#define MINK_AVM_SOCKET_NAME MINK_QRTR_SERVICE_FROM_LA

#define MINK_HLOS_SOCKET_NAME "5033"

#define PROCESS_LOADER_UID CTOEMVMProcessLoader_UID
#define POWER_SERVICE_UID CTOEMVMPowerService_UID
#define MEMPOOL_FACTORY_UID CTOEMVMMemPoolFactory_UID
#define PLATFORM_INFO_SERVICE_UID COEMVMPlatformInfo_UID
#define REBOOT_SERVICE_UID CTRebootOEMVM_UID
#define TACCESS_CONTROL_UID CTOEMVMAccessControl_UID

#define LOCAL_OSID "oemvm"

#ifndef OFFTARGET
// On Device

// definitions
#define MINK_QRTR_SERVICE_TO_LA MINK_QRTR_SERVICE_FROM_OEMVM
#define MINK_QRTR_SERVICE_TO_LA_VERSION MINK_QRTR_SERVICE_FROM_OEMVM_VERSION
#define MINK_QRTR_SERVICE_TO_LA_INSTANCE MINK_QRTR_SERVICE_FROM_OEMVM_INSTANCE

#define MINK_QRTR_SERVICE_FROM_OEMVM 5010
#define MINK_QRTR_SERVICE_FROM_OEMVM_VERSION 1
#define MINK_QRTR_SERVICE_FROM_OEMVM_INSTANCE 1

#define MINK_VSOCK_SERVICE_TO_XVM MINK_VSOCK_OEM_PORT_FROM_OEMVM
#define MINK_VSOCK_OEM_PORT_FROM_OEMVM 0x6667

#define MINK_QMSGQ_SERVICE_TO_XVM MINK_QMSGQ_OEM_PORT_FROM_OEMVM
#define MINK_QMSGQ_OEM_PORT_FROM_OEMVM 0x6544

/* VSOCK */
#define MINK_VSOCK_SERVICE_FROM_XVM MINK_VSOCK_OEM_PORT_FROM_TVM
#define MINK_VSOCK_OEM_PORT_FROM_TVM "26214"  // (0x6666)

#define MINK_XVM_VSOCK_NAME MINK_VSOCK_SERVICE_FROM_XVM
#define MINK_XVM_VSOCK_TYPE MINKHUB_VSOCK

/* QMSGQ */
#define MINK_QMSGQ_SERVICE_FROM_XVM MINK_QMSGQ_OEM_PORT_FROM_TVM
#define MINK_QMSGQ_OEM_PORT_FROM_TVM "25923"  // (0x6543)

#define MINK_XVM_QMSGQ_NAME MINK_QMSGQ_SERVICE_FROM_XVM
#define MINK_XVM_QMSGQ_TYPE MINKHUB_QMSGQ

#define MINK_AVM_SOCKET_TYPE MINKHUB_QIPCRTR

// Unix sockets for local connections
#define SOCKET_DIR_NAME "/dev/socket/"
#define ROOT_SOCKET_FILE_NAME "root"
#define PRELAUNCHER_SOCKET_FILE_NAME "prelauncher"
#define MINKD_SOCKET_FILE_NAME "minkd"

// Daemon Name
#define PRELAUNCHER_DAEMON_NAME "TVMPrelauncher"
#define MINK_DAEMON_NAME "TVMMink"

// Configure file Dir
#define CONFIGURE_DIR "/etc/profiles"

#else
// OFFTARGET

// Sockets to other VM
#define MINK_VSOCK_SIMULATED_FD_ENV_VAR "not_applicable"
#define MINK_VSOCK_SIMULATED_SOCKET_NAME "oemvm_vsock_simulated_socket"
#define MINK_QRTR_SIMULATED_FD_ENV_VAR "not_applicable"
#define MINK_QRTR_SIMULATED_SOCKET_NAME "oemvm_qrtr_simulated_socket"
#define MINK_QMSGQ_SIMULATED_FD_ENV_VAR "not_applicable"
#define MINK_QMSGQ_SIMULATED_SOCKET_NAME "oemvm_qmsgq_simulated_socket"

// Sockets from other VM
#define MINK_VSOCK_SERVICE_FROM_XVM_SIMULATED "tvmd_vsock_simulated_socket"
#define MINK_XVM_VSOCK_NAME MINK_VSOCK_SERVICE_FROM_XVM_SIMULATED
#define MINK_XVM_VSOCK_TYPE MINKHUB_SIMULATE

#define MINK_QMSGQ_SERVICE_FROM_XVM_SIMULATED "tvmd_qmsgq_simulated_socket"
#define MINK_XVM_QMSGQ_NAME MINK_QMSGQ_SERVICE_FROM_XVM_SIMULATED
#define MINK_XVM_QMSGQ_TYPE MINKHUB_SIMULATE

#define MINK_AVM_SOCKET_TYPE MINKHUB_UNIX

// Unix sockets for local connections
#define SOCKET_DIR_NAME "./"
#define ROOT_SOCKET_FILE_NAME "oemvm_root_socket"
#define PRELAUNCHER_SOCKET_FILE_NAME "oemvm_prelauncher_socket"
#define MINKD_SOCKET_FILE_NAME "oemvm_socket"

// Daemon Name
#define PRELAUNCHER_DAEMON_NAME "OEMPrelauncher"
#define MINK_DAEMON_NAME "OEMMink"

// Configure file Dir
#define CONFIGURE_DIR "profiles/off-target/oemvm"

#endif

#define ROOT_SOCKET_NAME SOCKET_DIR_NAME ROOT_SOCKET_FILE_NAME
#define PRELAUNCHER_SOCKET_NAME SOCKET_DIR_NAME PRELAUNCHER_SOCKET_FILE_NAME
#define MINKD_SOCKET_NAME SOCKET_DIR_NAME MINKD_SOCKET_FILE_NAME

#define VM_UUID CLIENT_VMUID_OEM

#endif  // __PLATFORM_CONFIG_H
