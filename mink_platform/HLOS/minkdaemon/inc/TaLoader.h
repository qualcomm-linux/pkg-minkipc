// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TALOADER_H
#define __TALOADER_H

#include <string>
#include <vector>
#include "object.h"

void loadApps(Object tEnvObj, std::vector<Object> &loadedApps, std::string const &config_path);
void *loadSym(void *handle, const char *sym);

#endif  // __TALOADER_H
