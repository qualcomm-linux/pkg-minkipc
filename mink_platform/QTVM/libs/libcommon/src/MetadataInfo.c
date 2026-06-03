// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "MetadataInfo.h"

#include <openssl/sha.h>
#include "HexUtils.h"
#include "ICredentials.h"
#include "KMemPort.h"
#include "MDScan.h"
#include "MemSCpy.h"
#include "MinkTypes.h"
#include "TUtils.h"
#include "cdefs.h"
#include "heap.h"
#include "object.h"
#include "qlist.h"

#define MDInfo_MAX_APP_NAME_LEN 32
#define IDLIST_DEFAULT_EXPAND_SIZE 10

typedef struct {
    uint32_t capacity;
    uint32_t index;
    uint32_t *id;
} IDList;

struct MetadataInfo {
    void *metaData;
    size_t metaDataSize;

    IDList privileges;
    IDList hostedServices;

    // Values parsed from metadata
    char appName[MDInfo_MAX_APP_NAME_LEN];
    char *domain;
    bool neverUnload;

    // Other values
    DistId distId;
    char *distName;
    // TP ROT
    uint32_t taRot;

    // Meta meta data, internal bitmask
    uint32_t flags;
};

// Definition of supported information keys
enum {
    // Metadata keys
    eName = 'n',         // Name
    eNeverUnload = 'U',  // NeverUnload

    // Sideloaded keys
    eDistinguishedID = ('d' << 16 | 'i' << 8 | 'd'),  // Distinguished id
    eDomain = ('d' << 16 | 'm' << 8 | 'n'),           // Domain
    eRot = ('r' << 16 | 'o' << 8 | 't'),              // Root Of Trust
};

// Bitmask position for info->flags
enum {
    indAppName = 0,       // appName set?
    indDistID = 4,        // distID ...
    indDomain = 5,        // domain ...
    indRot = 7,           // ROT
    indProcID = 9,        // process ID set?
    indNeverUnload = 10,  // neverUnload ...
} MD_FLAGS;

void MetadataInfo_destruct(MetadataInfo *me)
{
    if (!me) {
        return;
    }

    HEAP_FREE_PTR_IF(me->privileges.id);
    HEAP_FREE_PTR_IF(me->hostedServices.id);
    HEAP_FREE_PTR(me->metaData);
    HEAP_FREE_PTR(me->domain);
    HEAP_FREE_PTR(me->distName);
    HEAP_FREE_PTR(me);
}

// Fwd declaration of local functions
static void _generateDistinguishedId(MetadataInfo *me);
static char *_generateDistinguishedName(MetadataInfo *me);
static inline unsigned int _strKey(const char *key, size_t length);

/**
 * Description: Given an index, return the name (key) and value pair.
 *
 * In:          index: Index of key/value pair array.
 *              nameLen: Length (in bytes) of name.
 *              valueLen: Length (in bytes) of value.
 *
 * Out:         name: Name (key) of key/value pair.
 *              nameLenOut: Actual length of name.
 *              value: Value of key/value pair.
 *              valueLenOut: Actual length of value.
 *
 * Return:      Object_OK on success.
 *              All else on failure.
 */
int32_t MetadataInfo_getPropertyByIndex(MetadataInfo *me, uint32_t index, void *name,
                                        size_t nameLen, size_t *nameLenOut, void *value,
                                        size_t valueLen, size_t *valueLenOut)
{
    MDScan md;
    uint32_t scanIndex = 0;

    MDScan_init(&md, me->metaData, me->metaDataSize);

    while (MDScan_next(&md)) {
        if (scanIndex == index) {
            size_t nameTotal = md.name.len + 1;
            if (nameTotal > nameLen) {
                return ICredentials_ERROR_NAME_SIZE;
            }

            size_t valueTotal = MDScan_readString(&md, value, valueLen);
            if (valueTotal > valueLen) {
                return ICredentials_ERROR_VALUE_SIZE;
            }

            *valueLenOut = valueTotal;
            *nameLenOut = nameTotal;
            ((uint8_t *)name)[nameTotal - 1] = '\0';
            memscpy(name, nameLen, md.name.ptr, md.name.len);
            return Object_OK;
        }

        ++scanIndex;
    }

    return ICredentials_ERROR_NOT_FOUND;
}

/**
 * Description: Given a name (key), return the corresponding value.
 *
 * In:          name: Name (key) of key/value pair.
 *              nameLen: Length (in bytes) of name.
 *              valueLen: Length (in bytes) of value.
 *
 * Out:         value: Value of key/value pair.
 *              valueLenOut: Actual length of value.
 * Return:      Object_OK on success.
 *              All else on failure.
 */
int32_t MetadataInfo_getValueByName(MetadataInfo *me, const void *name, size_t nameLen, void *value,
                                    size_t valueLen, size_t *valueLenOut)
{
    MDScan md;
    int32_t retValue = ICredentials_ERROR_NOT_FOUND;

    void *data = NULL;
    size_t data_size = 0;

    switch (_strKey(name, nameLen)) {
        case eName:  // Name
        {
            if (BIT_TST(me->flags, indAppName)) {
                data = &me->appName;
                data_size = strlen(me->appName);
            }

            break;
        }

        case eNeverUnload:  // NeverUnload
        {
            if (BIT_TST(me->flags, indNeverUnload)) {
                data = &me->neverUnload;
                data_size = sizeof(me->neverUnload);
            }

            break;
        }

        case eDistinguishedID:  // Distinguished ID
        {
            if (!BIT_TST(me->flags, indDistID)) {
                _generateDistinguishedId(me);
            }

            if (BIT_TST(me->flags, indDistID)) {
                data = &me->distId;
                data_size = sizeof(me->distId);
            }

            break;
        }

        case eDomain:  // Domain path
        {
            if (BIT_TST(me->flags, indDomain)) {
                data = me->domain;
                data_size = strlen(me->domain) + 1;
            }

            break;
        }

        case eRot:  // ROT
        {
            if (BIT_TST(me->flags, indRot)) {
                data = &me->taRot;
                data_size = sizeof(me->taRot);
            }

            break;
        }

        default: {
            break;
        }
    }

    if (NULL != data && 0 != data_size) {
        if (memscpy(value, valueLen, data, data_size) == data_size) {
            *valueLenOut = data_size;
            retValue = Object_OK;
        } else {
            retValue = ICredentials_ERROR_VALUE_SIZE;
        }
    }

    if (Object_isOK(retValue) || (retValue != ICredentials_ERROR_NOT_FOUND)) {
        return retValue;
    }

    // MetadataInfo encapsulates all the metadata that the kernel uses, but
    // metadata may be extended in ways the kernel does not know.  If not found
    // in MetadataInfo, generically scan the metadata for the name-value pair.
    MDScan_init(&md, me->metaData, me->metaDataSize);

    while (MDScan_next(&md)) {
        if (0 == tmemscmp(md.name.ptr, md.name.len, name, nameLen)) {
            // This is the requested property
            size_t valueTotal = MDScan_readString(&md, value, valueLen);
            if (valueTotal > valueLen) {
                return ICredentials_ERROR_VALUE_SIZE;
            }

            *valueLenOut = valueTotal;
            return Object_OK;
        }
    }

    return ICredentials_ERROR_NOT_FOUND;
}

int32_t MetadataInfo_getServices(MetadataInfo *me, const uint32_t **services, uint32_t *serviceLen)
{
    int32_t ret = Object_OK;

    T_CHECK_ERR(services != NULL && serviceLen != NULL, Object_ERROR_INVALID);

    *services = me->hostedServices.id;
    *serviceLen = me->hostedServices.index;

exit:
    return ret;
}

int32_t MetadataInfo_getPrivileges(MetadataInfo *me, const uint32_t **privileges,
                                   uint32_t *privilegeLen)
{
    int32_t ret = Object_OK;

    T_CHECK_ERR(privileges != NULL && privilegeLen != NULL, Object_ERROR_INVALID);

    *privileges = me->privileges.id;
    *privilegeLen = me->privileges.index;

exit:
    return ret;
}

/**
 *  Description: Set the authentication domain for the module.
 *               The domain is a null terminated c string which is added to
 *               other tmodule information in order to generate the tmodule's
 *               distinguished id.
 *
 *  In:          me: pointer of MetadataInfo structure
 *               domain: authentication string
 *  Return:      Object_OK on success.
 *               Object_ERROR_MEM if heap_zalloc fail.
 *               Object_ERROR otherwise.
 */
int32_t MetadataInfo_setDomain(MetadataInfo *me, char const *domain)
{
    if (BIT_TST(me->flags, indDomain)) {
        return Object_ERROR;
    }

    size_t domain_len = strlen(domain) + 1;

    me->domain = (char *)heap_zalloc(domain_len);
    if (NULL == me->domain) {
        return Object_ERROR_MEM;
    }

    // Had trouble with strlcpy on LE/Offtarget
    domain_len = domain_len - 1;
    memscpy(me->domain, domain_len, domain, domain_len);
    me->domain[domain_len] = '\0';

    BIT_SET(me->flags, indDomain);
    return Object_OK;
}

const char *MetadataInfo_getDomain(MetadataInfo *me)
{
    return me->domain;
}

/**
 *  Description: Set the authentication root of trust for the module.
 *
 *  In:          me: pointer of MetadataInfo structure
 *               taRot: Root of Trust
 *  Return:      Object_OK on success.
 *               Object_ERROR otherwise.
 */
int32_t MetadataInfo_setRot(MetadataInfo *me, uint32_t taRot)
{
    me->taRot = taRot;
    BIT_SET(me->flags, indRot);
    return Object_OK;
}

uint32_t MetadataInfo_getRot(MetadataInfo *me)
{
    return me->taRot;
}

/**
 *  Description: Get the name of the module, set in the metadata.
 *
 *  In:          me: pointer of MetadataInfo structure
 *  Return:      Null-terminated string of name.
 */
const char *MetadataInfo_getName(MetadataInfo *me)
{
    if (!BIT_TST(me->flags, indAppName)) {
        return NULL;
    }

    return me->appName;
}

/**
 *  Description: Get the Distinguished Name for the module.
 *
 *  In:          me: pointer of MetadataInfo structure
 *  Return:      pointer to DistName on success.
 *               NULL otherwise.
 */
const char *MetadataInfo_getDistName(MetadataInfo *me)
{
    if (NULL == me->distName) {
        _generateDistinguishedName(me);
    }

    return me->distName;
}

int32_t MetadataInfo_getDistId(MetadataInfo *me, DistId *distId)
{
    if (!BIT_TST(me->flags, indDistID)) {
        _generateDistinguishedId(me);
    }

    if (!BIT_TST(me->flags, indDistID)) {
        return Object_ERROR;
    }

    *distId = me->distId;
    return Object_OK;
}

/**
 * Use MetadataInfo data to generate a unique textual name (distinguished name)
 * and then use this name as a source for creating a unique identifier
 * (distinguished id).
 *
 * The distinguished name is generated by adding data domain+TP name,
 * elements in the distinguished name are separated by a period ('.').
 *
 * Ex. <domain>.<name>
 *
 * @note The domain segment is generated by the owner of the TModule instance
 * with the requirement that the information in the domain element will result
 * in a unique distinguished id.
 *
// TODO: review relevance of the following description
 * @note For trusted domains, we only use the TP name and not the fully
 * qualified distinguished name. This is done for legacy reasons where TP name
 * is used as identity and is only allowed for domains where we can ensure no
 * conflicts can occur.
 *
 * Returns a pointer to a C string or NULL, this C string needs to be freed
 * by the invoker.
 *
 * @note There are two versions of this function (see
 * generateDistinguishedNameLegacy for more information)
 */
static char *_generateDistinguishedName(MetadataInfo *me)
{
    size_t len = 0;

    // Check that domain and appname exists.
    if (!BIT_TST(me->flags, indAppName) || !BIT_TST(me->flags, indDomain)) {
        return NULL;
    }

    // Already generated
    if (NULL != me->distName) {
        return me->distName;
    }

    // TODO
    bool trusted = false;

    // Processes from untrusted domains need to be fully qualified.
    if (!trusted) {
        len = strlen((const char *)me->domain);
        len++;  // '.' separator
    }

    // Add name to the id hash
    len += strlen(me->appName);  // TP Name

    len++;  // +terminator

    me->distName = HEAP_ZALLOC_ARRAY(char, len);
    if (NULL == me->distName) {
        return NULL;
    }

    // The '.' is only needed for non-trusted. Need to fix this when above
    // to-do is fixed for trusted
    snprintf(me->distName, len, "%s.%s", me->domain, me->appName);

    return me->distName;
}

/**
 * This is a helper function that generates a unique id (SHA256) based on
 * textual data.
 */
static void _generateDistinguishedId(MetadataInfo *me)
{
    char *didName = NULL;
    uint8_t digest[SHA256_DIGEST_LENGTH] = {0};

    didName = _generateDistinguishedName(me);

    if (didName) {
        if (0 == strlen(didName)) {  // no terminator
            return;
        }

        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, didName, strlen(didName));
        SHA256_Final(digest, &sha256);

        memscpy(&me->distId, sizeof(me->distId), digest, SHA256_DIGEST_LENGTH);
        BIT_SET(me->flags, indDistID);
    }

    return;
}

/**
 * Convert a 1-4 letter string into a ID by using the character code shifted
 * into a 32bit value based on the characters position.
 *
 *  In:          key: String to convert
 *               length: Length of key
 *  Return:      A positive integer or 0 on failure.
 */
static inline unsigned int _strKey(const char *key, size_t length)
{
    if (0 == length || sizeof(unsigned int) < length) {
        return 0;
    }

    size_t idx = 0;
    uint32_t key_val = 0;

    while (key[idx] && idx < length) {
        key_val <<= 8;

        key_val |= key[idx++];
    };

    return key_val;
}

static int32_t _insertId(IDList *idList, uint32_t newId)
{
    int32_t ret = Object_OK;
    uint32_t *tmp = NULL;

    if (idList->index == idList->capacity) {
        tmp = HEAP_ZALLOC_ARRAY(uint32_t, idList->capacity + IDLIST_DEFAULT_EXPAND_SIZE);
        T_CHECK_ERR(tmp != NULL, Object_ERROR_MEM);

        if (idList->capacity != 0) {
            memcpy(tmp, idList->id, idList->capacity * sizeof(uint32_t));
            HEAP_FREE_PTR(idList->id);
        }

        idList->id = tmp;
        idList->capacity += IDLIST_DEFAULT_EXPAND_SIZE;
    }

    idList->id[idList->index++] = newId;

exit:
    return ret;
}

/**
 *  Description: Initialize privileges, hosted services, and other metadata.
 *
 *  In:          me: pointer of MetadataInfo structure
 *               dataPtr: metadata string
 *               dataLen: Length of metadata string
 *  Return:      Object_OK on success.
 *               Object_ERROR otherwise.
 */
static int32_t _parseMetaData(MetadataInfo *me, char const *dataPtr, size_t dataLen)
{
    MDScan md;
    int32_t ret = Object_OK;

    MDScan_init(&md, dataPtr, dataLen);

    while (MDScan_next(&md)) {
        char nameChar = md.name.ptr[0];

        switch (nameChar) {
            case eName:  // name
                (void)MDScan_readString(&md, me->appName, sizeof(me->appName));
                BIT_SET(me->flags, indAppName);
                break;

            case eNeverUnload:  // neverUnload
                if (md.value.len >= 1 && (0 == memcmp(md.value.ptr, "true", md.value.len))) {
                    me->neverUnload = true;
                }
                BIT_SET(me->flags, indNeverUnload);
                break;

            case 'p':  // privileges
            case 's':  // services hosted/advertised
            {
                IDList *idList = (nameChar == 'p' ? &me->privileges : &me->hostedServices);
                IDScan idscan;

                IDScan_init(&idscan, &md);
                while (IDScan_next(&idscan)) {
                    T_GUARD(_insertId(idList, idscan.id));
                }

                break;
            }

            default:
                break;
        }
    }

    // Optional values:
    if (!BIT_TST(me->flags, indNeverUnload)) {
        BIT_SET(me->flags, indNeverUnload);
        me->neverUnload = false;
    }

exit:
    return ret;
}

int32_t MetadataInfo_new(MetadataInfo **meOut, char const *metaData, size_t metaDataSize)
{
    int32_t ret = Object_ERROR;
    MetadataInfo *me = HEAP_ZALLOC_TYPE(MetadataInfo);

    T_CHECK_ERR(me, Object_ERROR_MEM);
    T_CHECK(metaData);
    T_CHECK(metaDataSize > 0);

    me->metaData = (void *)HEAP_ZALLOC_ARRAY(uint8_t, metaDataSize);
    T_CHECK_ERR(me->metaData, Object_ERROR_MEM);

    memcpy(me->metaData, metaData, metaDataSize);
    me->metaDataSize = metaDataSize;

    T_GUARD(_parseMetaData(me, (const char *)me->metaData, me->metaDataSize));

    *meOut = me;
    return Object_OK;

exit:
    MetadataInfo_destruct(me);
    return ret;
}
