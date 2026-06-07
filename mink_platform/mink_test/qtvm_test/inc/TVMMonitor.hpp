// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <stdint.h>
#include <map>
#include <memory>
#include <string>

#pragma once

#define TRUSTED_VM std::string("trustedvm")
#define OEM_VM std::string("oemvm")

class VirtClientInterface;

class TVMMonitor
{
   public:
    TVMMonitor()
    {
    }
    ~TVMMonitor()
    {
        mVirtClientInterface.clear();
    }
    int32_t startVM(std::string VMName);
    void shutdownVM(std::string VMName);

   private:
    int32_t connectVM(std::string VMName);
    std::map<std::string, std::shared_ptr<VirtClientInterface>> mVirtClientInterface;
};
