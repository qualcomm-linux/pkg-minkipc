// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CREDENTIALSMANAGER_H
#define __CREDENTIALSMANAGER_H

#include <map>
#include <mutex>
#include <string>
#include <thread>
#include "EnvWrapper.h"
#include "object.h"

struct EnvPair {
    Object creds;
    Object envWrapper;
    EnvPair() : creds(Object_NULL), envWrapper(Object_NULL){};

    // All Explicit Retained Copies should be released
    // when this destructor is called
    ~EnvPair()
    {
        // Release any cached Credentials
        LOG_MSG("Deleting EnvPair");
        Object_ASSIGN_NULL(creds);
        Object_ASSIGN_NULL(envWrapper);
    }
};

class CredentialsManager
{
   private:
    std::mutex _mutex;
    int32_t _qteeObjectQuota;
    int32_t _maxQTEEObjectsPerClient;
    int32_t _maxSystemClients;
    std::map<std::string, EnvPair *> _credMap;

   public:
    void cleanEnvs();
    bool add(const void *client_id, size_t client_id_len, const void *credential_buf,
             size_t cred_buf_len, const Object env, const Object credentials, Object *envWrapper);
    bool get(const void *client_id, size_t client_id_len, Object *envWrapper);

    size_t getNumSystemClients()
    {
        return _credMap.size();
    }

    void updateMaxObjectPerClient(int32_t objectsPerClient);
    void updateMaxSystemClients(int32_t sysClients);
    void updateMaxSystemObjQuota(int32_t systemQuota);

    bool maxSystemClientConnected()
    {
        if (_maxSystemClients > _credMap.size())
            return false;
        else
            return true;
    }

    CredentialsManager(int32_t maxObjectLimit, int32_t maxClients, int32_t maxSystemObjLimit)
        : _qteeObjectQuota(maxSystemObjLimit),
          _maxQTEEObjectsPerClient(maxObjectLimit),
          _maxSystemClients(maxClients)
    {}

    ~CredentialsManager()
    {
        LOG_MSG("Releasing All Credentials %d", _credMap.size());
        for (auto it = _credMap.begin(); it != _credMap.end(); it++) {
            LOG_MSG("Releasing Cred for ClientID %s with EnvPair %p", it->first.c_str(),
                    it->second);
            delete (it->second);
        }
    }
};

#endif  // __CREDENTIALSMANAGER_H
