// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __HLOSMINKDAEMON_CONFIG_H
#define __HLOSMINKDAEMON_CONFIG_H

#define ENABLE_QRTR_SERVER true
#define HLOSMINKD_OPENER_SOCKET_NAME "ANDROID_SOCKET_hlos_mink_opener"

#define SSGTZD_FALLBACK_SOCKET_LOCATION "/dev/socket/ssgtzd"
#define SSGTZD_SOCKET_NAME "ANDROID_SOCKET_ssgtzd"

#ifndef OFFTARGET
#define HLOSMINKD_OPENER_FALLBACK_SOCKET_NAME "/dev/socket/hlos_mink_opener"
#define MINK_QTVM_SOCKET_PORT "5008"
#define MINK_OEMVM_SOCKET_PORT "5010"

#define MINK_QRTR_SERVICE_FROM_LA 5033
#define MINK_QRTR_SERVICE_FROM_LA_VERSION 1
#define MINK_QRTR_SERVICE_FROM_LA_INSTANCE 1

#define MINK_IPCR_MODEM_SERVICE 5013
#define MINK_IPCR_MODEM_SERVICE_VERSION 1
#define MINK_IPCR_MODEM_SERVICE_INSTANCE 1
#else
#define HLOSMINKD_OPENER_FALLBACK_SOCKET_NAME "simulated_hlos_mink_opener"
#define MINK_QRTR_SIMULATED_QTVM_NAME "tvm_qrtr_simulated_socket"
#define MINK_QRTR_SIMULATED_OEMVM_NAME "oemvm_qrtr_simulated_socket"

#define MINK_QRTR_SERVICE_FROM_LA "5033"
#define MINK_AVM_SOCKET_NAME MINK_QRTR_SERVICE_FROM_LA
#endif

#endif  //__HLOSMINKDAEMON_CONFIG_H
