// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __ENVWRAPPER_H
#define __ENVWRAPPER_H

#include <stdlib.h>
#include <list>
#include <memory>
#include <mutex>
#include "IClientEnv.h"
#include "MinkDaemon_logging.h"
#include "object.h"

// Identifiier for Parent EnvWrapper
#define OBJTYPE_ENVWRAPPER 0xF0F0F0F0
#define OBJTYPE_OBJWRAPPER 0xFFFF0000

// Iterator for different Sections
#define FOR_ARGS(ndxvar, counts, section)                                                    \
    for (size_t ndxvar = ObjectCounts_index##section(counts);                                \
         ndxvar < (ObjectCounts_index##section(counts) + ObjectCounts_num##section(counts)); \
         ++ndxvar)

// Expose private member for Unit Testing
#ifdef ENABLE_UNIT_TEST
#define private public
#endif  // ENABLE_UNIT_TEST

// Default configuration
#define DEFAULT_SYSTEM_CLIENTS 12
// Max Possible Values
#define MAX_SYSTEM_CLIENTS 20
// Default Object per Client
#define DEFAULT_OBJS_PER_CLIENT 20
// System Quota for Objects that acan be used by system clients
#define DEFAULT_SYSTEM_QUOTA 100
// Maxminum Object per Client
#define MAX_ALLOWED_OBJS_ACROSS_SYSCLIENT 30
// Maximum Object Cap per System
#define MAX_ALLOWED_SYSTEM_OBJECTS 170

/**
 * Wrapper for System Objects Count
 *
 * mLock:                             mutex Lock for thread safety
 * globalQTEEObjectCountLimit:        Global System Object Count
 */
struct qteeMetaInfoWrapper {
    std::mutex mLock;
    int32_t globalQTEEObjectCountLimit;
    qteeMetaInfoWrapper() : globalQTEEObjectCountLimit(1){};
    ~qteeMetaInfoWrapper(){};
};

class sysObjQuota
{
   private:
    // qteeInfo is only owned by sysObjQuota class hence using a unique_ptr
    std::unique_ptr<qteeMetaInfoWrapper> qteeInfo;
    // Constructor: Initialize the Unique pointer to qteeInfoMetaWrapper
    sysObjQuota()
    {
        // During Destruction this pointer is released and object is destroyed.
        qteeInfo = std::make_unique<qteeMetaInfoWrapper>();
    };

   public:
    // Instance Getter
    static std::shared_ptr<sysObjQuota> getInstance()
    {
        static std::shared_ptr<sysObjQuota> qteeQuotaMonitoring(new sysObjQuota());
        return qteeQuotaMonitoring;
    }

    // Get qteeInfo Pointer
    struct qteeMetaInfoWrapper* getQTEEObjPtr()
    {
        return qteeInfo.get();
    }

    // Get Current QTEE ObjectCount
    int32_t getCurrentObjCount()
    {
        if (!qteeInfo) {
            LOG_ERR("Error : getCurrentObjCount() :: Invalid qteeInfo");
        } else {
            LOG_MSG("Total Object Count %d", qteeInfo->globalQTEEObjectCountLimit);
            std::lock_guard<std::mutex> guard(qteeInfo->mLock);
            return qteeInfo->globalQTEEObjectCountLimit;
        }

        return 0;
    }

    // Increment Object Count
    void incrementObjectCount()
    {
        if (!qteeInfo) {
            LOG_ERR("Error : incrementObjectCount() :: Invalid qteeInfo");
        } else {
            std::lock_guard<std::mutex> guard(qteeInfo->mLock);
            (void)atomic_add(&qteeInfo->globalQTEEObjectCountLimit, 1);
        }
    }

    // Decrement Object Count
    void decrementObjectCount()
    {
        if (!qteeInfo) {
            LOG_ERR("Error : decrementObjectCount() :: Invalid qteeInfo");
        } else {
            LOG_MSG("Total Object Count %d", qteeInfo->globalQTEEObjectCountLimit);
            std::lock_guard<std::mutex> guard(qteeInfo->mLock);
            if (qteeInfo->globalQTEEObjectCountLimit > 0)
                (void)atomic_add(&qteeInfo->globalQTEEObjectCountLimit, -1);
        }
    }

    // Copy, Move constructor, Assignment operator deleted
    sysObjQuota(const sysObjQuota&) = delete;
    sysObjQuota(sysObjQuota&&) = delete;
    sysObjQuota& operator=(const sysObjQuota&) = delete;
    sysObjQuota& operator=(sysObjQuota&&) = delete;

    ~sysObjQuota()
    {
        LOG_MSG("Resource Freed");
        // Release the qteeInfo pointer memory
        qteeInfo.reset();
    }
};

struct EnvObjWrapper {
    uint32_t parentWrapperType;
    size_t parentWrapper;
    Object obj;
    int32_t refs;
    EnvObjWrapper() : parentWrapperType(OBJTYPE_ENVWRAPPER), parentWrapper(0){};
    ~EnvObjWrapper()
    {
        LOG_MSG("Deleting ObjWrapper");
    }
};

struct EnvWrapper {
    struct EnvObjWrapper mClientEnvWrapper;
    int32_t qteeObjectCount;
    int32_t maxSystemObjectLimit = DEFAULT_SYSTEM_QUOTA;
    int32_t maxQTEEObjectLimit = DEFAULT_OBJS_PER_CLIENT;
    std::shared_ptr<sysObjQuota> objQuotaInfo;

    bool objLimitReached()
    {
        LOG_MSG("Max Object Count Limit: %d, Actual Object Count: %d", maxQTEEObjectLimit,
                qteeObjectCount);
        if (maxQTEEObjectLimit > qteeObjectCount)
            return false;
        else
            return true;
    }

    bool systemObjQuotaReached()
    {
        if (maxSystemObjectLimit > objQuotaInfo->getCurrentObjCount())
            return false;
        else
            return true;
    }

    void incrementObjCount()
    {
        objQuotaInfo->incrementObjectCount();
        (void)atomic_add(&qteeObjectCount, 1);
    }

    void decrementObjCount()
    {
        // Decrement Count as the Object associated with the EnvObjWrapper is bieng Released
        objQuotaInfo->decrementObjectCount();
        if (qteeObjectCount > 0) {
            (void)atomic_add(&qteeObjectCount, -1);
        }
    }

    EnvWrapper(int32_t MaxObjectLimit, int32_t MaxSystemQuota)
        : qteeObjectCount(1),
          maxSystemObjectLimit(MaxSystemQuota),
          maxQTEEObjectLimit(MaxObjectLimit),
          objQuotaInfo(sysObjQuota::getInstance())
    {
        mClientEnvWrapper.refs = 1;
        mClientEnvWrapper.obj = Object_NULL;
        mClientEnvWrapper.parentWrapper = (size_t)this;
        LOG_MSG("qteeInfoWrapper %p", objQuotaInfo->getQTEEObjPtr());
        LOG_MSG("Constructed EnvObjWrapper addr %p", &mClientEnvWrapper);
    }

    // Although there is a check for EnvWrapper to only be destructed when
    // mClientEnvWrapper.refs is 0 , still adding a destructor clean up.
    ~EnvWrapper()
    {
        if (!Object_isNull(mClientEnvWrapper.obj)) {
            Object_release(mClientEnvWrapper.obj);
        }
    }
};

int32_t EnvWrapper_open(const Object env, Object* objOut, int32_t qteeObjectLimit,
                        int32_t quotaLimit);
int32_t EnvWrapper_getQteeObjCount(const Object* me);
int32_t EnvObjWrapper_getRefs(const Object* me);

#endif  // __ENVWRAPPER_H
