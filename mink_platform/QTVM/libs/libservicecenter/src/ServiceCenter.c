// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#ifndef OFFTARGET
#include <yaml.h>
#endif

#include "EmbeddedProcessIDs.h"
#include "EmbeddedProcessList.h"
#include "MemSCpy.h"
#include "ServiceCenter.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"
#include "object.h"
#include "qlist.h"

#define PATH_MAX_LEN 100
#define DATA_MAX_LEN 128

#ifndef OFFTARGET
#define CONFIGURE_FILE_SUFFIX ".yaml"
#else
#define CONFIGURE_FILE_SUFFIX ".conf"
#endif

#define GET_SERVICE_TYPE(x) (1 << (x))
#define SERVICE_CID_TAG "id"
#define SERVICE_AUTOSTART_TAG "autostart"
#define TRUE "true"

typedef struct {
    SERVICE_TYPE type;
    uint32_t cid;
    uint32_t uid;
    int32_t refs;
    bool autoStart;
    char path[PATH_MAX_LEN];
    QNode node;
} TServiceInfo;

static QLIST_DEFINE_INIT(gServiceList);
static uint32_t gAutoStartServiceCount = 0;
static pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;

static TServiceInfo *_constructServiceInfo(void)
{
    int32_t ret = Object_OK;
    TServiceInfo *info = NULL;

    info = HEAP_ZALLOC_TYPE(TServiceInfo);
    T_CHECK(info != NULL);

    info->autoStart = false;
    info->refs = 1;

exit:
    return info;
}

static void _destructServiceInfo(TServiceInfo *info)
{
    if (info == NULL) {
        return;
    }

    QNode_dequeueIf(&info->node);
    HEAP_FREE_PTR(info);
}

static int32_t _getServiceType(cid_t cid, SERVICE_TYPE *type)
{
    if (cid < DL_BASE) {
        *type = EMBEDDED_DAEMON;
    } else if (cid < EMBEDDED_BASE) {
        *type = DOWNLOADABLE_SERVICE;
    } else if (cid < CID_ENUM_MAX) {
        *type = EMBEDDED_SERVICE;
    } else {
        return Object_ERROR_INVALID;
    }

    return Object_OK;
}

static int32_t _parseServiceInfo(TServiceInfo *info)
{
    int32_t ret = Object_ERROR_INVALID;
    int32_t len = 0;

    T_GUARD(_getServiceType(info->cid, &info->type));

    for (int32_t i = 0; i < embeddedProcessIDCount; ++i) {
        if (embeddedProcessIDList[i].cid == info->cid) {
            info->uid = embeddedProcessIDList[i].uid;
            break;
        }
    }

    for (int32_t i = 0; i < embeddedProcessCount; ++i) {
        if (embeddedProcessList[i].cid == info->cid) {
            len = snprintf(info->path, sizeof(info->path) - 1, "%s", embeddedProcessList[i].path);
            T_CHECK_ERR(len == strlen(embeddedProcessList[i].path), Object_ERROR_INVALID);
            ret = Object_OK;
            break;
        }
    }

exit:
    return ret;
}

static bool _isMatchTag(char *data, int32_t length, const char *expectTag)
{
    return length == strlen(expectTag) && memcmp(data, expectTag, length) == 0;
}

static void _debugLogSerivce(TServiceInfo *info)
{
#ifdef QTVM_TEST
    LOG_MSG("Get Service 0x%x, cid(%u), type is %d, %s autoStartService, path is %s\n", info->uid,
            info->cid, info->type, info->autoStart ? "is" : "isn't", info->path);
#endif
}

static bool _isTargetType(int32_t type, int32_t targetServiceType)
{
    return ((1 << type) & targetServiceType) != 0;
}

#ifndef OFFTARGET
static void _parseServiceConfiguration(yaml_document_t *document, yaml_node_t *node,
                                       int32_t serviceType)
{
    int32_t ret = Object_OK;
    uint32_t data;
    TServiceInfo *info = NULL;

    info = _constructServiceInfo();
    T_CHECK(info != NULL);

    for (yaml_node_pair_t *node_pair = node->data.mapping.pairs.start;
         node_pair < node->data.mapping.pairs.top; ++node_pair) {
        yaml_node_t *key = yaml_document_get_node(document, node_pair->key);
        yaml_node_t *value = yaml_document_get_node(document, node_pair->value);

        T_CHECK(key != NULL && value != NULL);

        if (key->type != YAML_SCALAR_NODE || value->type != YAML_SCALAR_NODE) {
            continue;
        }

        if (_isMatchTag(key->data.scalar.value, key->data.scalar.length, SERVICE_CID_TAG)) {
            errno = 0;
            data = (uint32_t)strtol((const char *)value->data.scalar.value, NULL, 0);
            T_CHECK(errno == 0);
            info->cid = data;
            T_GUARD(_parseServiceInfo(info));
            continue;
        }

        if (_isMatchTag(key->data.scalar.value, key->data.scalar.length, SERVICE_AUTOSTART_TAG)) {
            info->autoStart =
                _isMatchTag(value->data.scalar.value, value->data.scalar.length, TRUE);
            gAutoStartServiceCount += info->autoStart ? 1 : 0;
            continue;
        }
    }

    T_CHECK(_isTargetType(info->type, serviceType));
    pthread_mutex_lock(&gLock);
    QList_appendNode(&gServiceList, &info->node);
    pthread_mutex_unlock(&gLock);
    _debugLogSerivce(info);

exit:
    if (Object_isERROR(ret)) {
        _destructServiceInfo(info);
    }

    return;
}

static int32_t _parseConfigureFile(const char *fileName, uint32_t serviceType)
{
    int32_t ret = Object_OK;
    FILE *file = NULL;
    bool done = false;
    yaml_parser_t parser = {0};
    yaml_document_t document = {0};

    if (!yaml_parser_initialize(&parser)) {
        LOG_MSG("Initialize yaml parser failed!\n");
        return Object_ERROR;
    }

    LOG_MSG("configure file = %s", fileName);
    file = fopen(fileName, "rb");
    T_CHECK(file != NULL);

    yaml_parser_set_input_file(&parser, file);

    while (!done) {
        T_CHECK(yaml_parser_load(&parser, &document) != 0);

        if (yaml_document_get_root_node(&document) != NULL) {
            yaml_char_t *lastScalar = NULL;
            size_t length;
            for (yaml_node_t *node = document.nodes.start; node < document.nodes.top; ++node) {
                switch (node->type) {
                    case YAML_SCALAR_NODE:
                        lastScalar = node->data.scalar.value;
                        length = node->data.scalar.length;
                        break;
                    case YAML_MAPPING_NODE:
                        if (lastScalar == NULL) {
                            continue;
                        }

                        if (_isMatchTag((char *)lastScalar, length, "service")) {
                            _parseServiceConfiguration(&document, node, serviceType);
                        }

                        lastScalar = NULL;
                        break;
                    default:
                        break;
                }
            }
        } else {
            done = true;
        }

        yaml_document_delete(&document);
    }

exit:
    yaml_parser_delete(&parser);
    if (file != NULL) {
        fclose(file);
    }

    return ret;
}
#else
static void _parseServiceConfiguration(char *data, int32_t serviceType)
{
    int32_t ret = Object_OK;
    char *token;
    char *pos = data;
    TServiceInfo *info = NULL;

    info = _constructServiceInfo();
    T_CHECK(info != NULL);

    token = strsep(&pos, ";");
    T_CHECK(token != NULL);
    errno = 0;
    info->cid = (uint32_t)strtol((const char *)token, NULL, 0);
    T_CHECK(errno == 0);

    T_GUARD(_parseServiceInfo(info));

    if (info->type != DOWNLOADABLE_SERVICE) {
        // Last tag is autoStart tag which is an optional tag.
        token = strsep(&pos, ";");
        T_CHECK_ERR(token != NULL, Object_OK);

        info->autoStart = _isMatchTag(token, strlen(token), TRUE);
        gAutoStartServiceCount += info->autoStart ? 1 : 0;
    }

    T_CHECK(_isTargetType(info->type, serviceType));
    pthread_mutex_lock(&gLock);
    QList_appendNode(&gServiceList, &info->node);
    pthread_mutex_unlock(&gLock);
    _debugLogSerivce(info);

exit:
    if (Object_isERROR(ret)) {
        _destructServiceInfo(info);
    }
}

static int32_t _parseConfigureFile(const char *fileName, uint32_t serviceType)
{
    int32_t ret = Object_OK;
    char data[DATA_MAX_LEN] = {0};
    char *token;
    char *pos;
    FILE *file = NULL;

    LOG_MSG("configure file = %s", fileName);
    file = fopen(fileName, "r");
    T_CHECK(file != NULL);

    while (!feof(file) && fgets(data, sizeof(data), file) != NULL) {
        pos = data;

        if (strstr(data, "#") != NULL) {
            continue;
        }

        _parseServiceConfiguration(data, serviceType);
    }

exit:
    if (file != NULL) {
        fclose(file);
    }

    return ret;
}
#endif

int32_t ServiceCenter_loadServiceProfiles(const char *path, int32_t serviceTypeList)
{
    int32_t ret = Object_OK;
    DIR *dir = NULL;
    struct dirent *node;
    char fileName[PATH_MAX] = {0};

    dir = opendir(path);
    T_CHECK(dir != NULL);

    while ((node = readdir(dir)) != NULL) {
        if (strstr(node->d_name, CONFIGURE_FILE_SUFFIX)) {
            snprintf(fileName, PATH_MAX, "%s/%s", path, node->d_name);
            if (Object_isERROR(_parseConfigureFile(fileName, serviceTypeList))) {
                LOG_MSG("Parse configuration file %s failed!", fileName);
            }
        }
    }

exit:
    if (dir != NULL) {
        closedir(dir);
    }
    return ret;
}

void ServiceCenter_destory(void)
{
    QNode *pNode, *pNodeNext;
    pthread_mutex_lock(&gLock);
    QLIST_NEXTSAFE_FOR_ALL(&gServiceList, pNode, pNodeNext)
    {
        TServiceInfo *serviceInfo = c_containerof(pNode, TServiceInfo, node);
        _destructServiceInfo(serviceInfo);
    }
    pthread_mutex_unlock(&gLock);
}

int32_t ServiceCenter_getAutoStartServiceCount(void)
{
    int32_t count = 0;
    pthread_mutex_lock(&gLock);
    count = gAutoStartServiceCount;
    pthread_mutex_unlock(&gLock);
    return count;
}

int32_t ServiceCenter_getAutoStartServiceList(uint32_t *uidList, uint32_t *sizeLen)
{
    int32_t ret = Object_OK;
    QNode *pNode;
    int32_t index = 0;

    pthread_mutex_lock(&gLock);
    T_CHECK_ERR(uidList != NULL && *sizeLen >= gAutoStartServiceCount, Object_ERROR_INVALID);

    if (gAutoStartServiceCount != 0) {
        QLIST_FOR_ALL(&gServiceList, pNode)
        {
            TServiceInfo *serviceInfo = c_containerof(pNode, TServiceInfo, node);
            if (serviceInfo->autoStart) {
                uidList[index++] = serviceInfo->uid;
            }
        }
    }

    *sizeLen = gAutoStartServiceCount;

exit:
    pthread_mutex_unlock(&gLock);
    return ret;
}

int32_t ServiceCenter_findService(uint32_t uid, void **info)
{
    QNode *pNode;
    int32_t ret = Object_ERROR_INVALID;

    pthread_mutex_lock(&gLock);
    QLIST_FOR_ALL(&gServiceList, pNode)
    {
        TServiceInfo *serviceInfo = c_containerof(pNode, TServiceInfo, node);
        if (serviceInfo->uid == uid) {
            LOG_MSG("Find the service %x\n", serviceInfo->uid);
            *info = (void *)serviceInfo;
            ret = Object_OK;
            break;
        }
    }
    pthread_mutex_unlock(&gLock);

    return ret;
}

int32_t ServiceCenter_getServiceAttribute(void *info, SERVICE_ATTRIBUTE attr, uint8_t *data,
                                          uint32_t *dataLen)
{
    int32_t ret = Object_OK;
    void *buffer;
    int32_t bufferLen;
    TServiceInfo *me;

    T_CHECK_ERR(info != NULL && data != NULL && dataLen != NULL, Object_ERROR_INVALID);

    me = (TServiceInfo *)info;

    switch (attr) {
        case TYPE:
            buffer = (void *)&me->type;
            bufferLen = sizeof(me->type);
            break;
        case AUTO_START:
            buffer = (void *)&me->autoStart;
            bufferLen = sizeof(me->autoStart);
            break;
        case PATH:
            T_CHECK(me->path != NULL);
            buffer = (void *)me->path;
            bufferLen = strlen(me->path) + 1;
            break;
        default:
            T_GUARD(Object_ERROR_INVALID);
    }

    T_CHECK((memscpy(data, *dataLen, buffer, bufferLen)) == bufferLen);
    *dataLen = bufferLen;

exit:
    return ret;
}