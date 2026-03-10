// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "listener_mngr.h"

int main() {

	MSGD("qtee_supplicant: process entry PPID = %d\n", getppid());

	if(0 != start_listener_services()) {
		MSGE("ERROR: listeners registration failed\n");
		return -1;
	}

	MSGE("QTEE_SUPPLICANT RUNNING\n");
	pause();
	MSGD("qtee_supplicant: Process exiting!!!\n");
	return -1;
}
