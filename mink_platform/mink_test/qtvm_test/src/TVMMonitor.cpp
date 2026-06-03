// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "TVMMonitor.hpp"
#include <iostream>

#ifdef __ANDROID__
#include "VirtClientInterface.h"

using namespace std;

#else

#define VAR2STR(s) #s
#define VAL2STR(s) VAR2STR(s)

#define QTVM_PATH out
#define OEMVM_PATH oemvm
#define ROOT TVMRoot
#define PRELAUNCHER TVMPrelauncher
#define MINK TVMMink
#define XVM_ROOT XVMRoot
#define XVM_PRELAUNCHER XVMPrelauncher
#define XVM_MINK XVMMink

using namespace std;
enum class Result {
    SUCCESS,
    FAILURE,
};

class VirtClientInterface
{
   public:
    VirtClientInterface(string VMName);
    VirtClientInterface() : VirtClientInterface(TRUSTED_VM)
    {
    }
    ~VirtClientInterface();
    Result startVMToUserspace();
    Result stopVM();

   private:
    bool mIsStarted;
    string mVMName;
    string mBasePath;
    string mRootName;
    string mPrelauncherName;
    string mMinkName;
};

VirtClientInterface::VirtClientInterface(string VMName) : mIsStarted(false)
{
    if (VMName == OEM_VM) {
        mVMName = OEM_VM;
        mBasePath = VAL2STR(OEMVM_PATH);
        mRootName = VAL2STR(XVM_ROOT);
        mPrelauncherName = VAL2STR(XVM_PRELAUNCHER);
        mMinkName = VAL2STR(XVM_MINK);
    } else {
        mVMName = TRUSTED_VM;
        mBasePath = VAL2STR(QTVM_PATH);
        mRootName = VAL2STR(ROOT);
        mPrelauncherName = VAL2STR(PRELAUNCHER);
        mMinkName = VAL2STR(MINK);
    }
}

VirtClientInterface::~VirtClientInterface()
{
    if (mIsStarted) {
        stopVM();
    }
}

Result VirtClientInterface::startVMToUserspace()
{
    string rootPath = mBasePath + string("/") + mRootName;
    string cmd = rootPath + string(" &");
    // Launch "VM"
    cout << "Starting " << rootPath << " by " << cmd << endl;
    system(cmd.c_str());
    mIsStarted = true;
    return Result::SUCCESS;
}

Result VirtClientInterface::stopVM()
{
    string cmd;
    if (mIsStarted) {
        cout << "Killing " << mRootName << endl;
        cmd = string("pkill ") + mRootName + string(" || true");
        system(cmd.c_str());
        cout << "Killing " << mMinkName << endl;
        cmd = string("pkill ") + mMinkName + string(" || true");
        system(cmd.c_str());
        cout << "Killing " << mPrelauncherName << endl;
        cmd = string("pkill ") + mPrelauncherName + string(" || true");
        system(cmd.c_str());
        mIsStarted = false;
        return Result::SUCCESS;
    }

    return Result::FAILURE;
}
#endif

int32_t TVMMonitor::connectVM(string VMName)
{
    if (!mVirtClientInterface.count(VMName)) {
        mVirtClientInterface[VMName] = make_shared<VirtClientInterface>(VMName);
        if (mVirtClientInterface.find(VMName) == mVirtClientInterface.end()) {
            return -1;
        }
    }

    return 0;
}

int32_t TVMMonitor::startVM(std::string VMName)
{
    int32_t ret = 0;

    ret = connectVM(VMName);
    if (ret != 0) {
        cout << "Can't find VM " << VMName << endl;
        return -1;
    }

    // Start VM and vote in order to keep VM alive
    Result result = mVirtClientInterface[VMName]->startVMToUserspace();
    if (result != Result::SUCCESS) {
        cout << "Start " << VMName << " fail. Result = " << (int32_t)result << endl;
    } else {
        cout << "Start " << VMName << " successfully!" << endl;
    }

    return result == Result::SUCCESS ? 0 : -1;
}

void TVMMonitor::shutdownVM(std::string VMName)
{
    if (mVirtClientInterface[VMName] != nullptr) {
        mVirtClientInterface[VMName]->stopVM();
    }
}
