// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

// ObjectTable
//
// An object table keeps track of local objects held by a remote domain,
// while will identify them with small non-negative integers.  The object
// table also counts reference to handles (not to be confused with the
// internal reference counting performed by each object).  When the handle
// reference count goes to zero, the object table slot is freed.
//
// The object table suports the following operations:
//
//  addObject()      Store an object in a slot in the table, yielding a handle.
//  recoverObject()  Recover an object, given a handle.
//  retainHandle()   Increment the reference count.
//  releaseHandle()  Decrement the reference count, and free the slot when it
//                   reaches zero.
//

#ifndef __OBJECTTABLE_H
#define __OBJECTTABLE_H

#include "heap.h"
#include "object.h"

typedef struct {
    // An array of objects held by a remote domain.
    Object *objects;

    // An array of reference counts managed by the remote domain.
    int32_t *objectRefs;

    // Size of the objects[] and objectRefs[] arrays.
    size_t objectsLen;

    // Number of objects currently in the table
    size_t objectsNum;
} ObjectTable;

// Add an object to the table, assigning it a handle.
// If the object is already in the table, increment its reference counter and
// return the handle. Otherwise add it to the table with its 'ref' value
// to 1.
//
// On success, return Object_OK.
// On failure, return Object_ERROR_NOSLOTS.
//
static inline int ObjectTable_addObject(ObjectTable *me, Object obj, size_t *i)
{
    size_t objectsNum = 0;
    size_t firstFree = SIZE_MAX;  // invalid

    // iterate through the whole table
    for (size_t n = 0; n < me->objectsLen; ++n) {
        // first check if this object is in the table already
        if (!Object_isNull(me->objects[n])) {
            if (me->objects[n].context == obj.context && me->objects[n].invoke == obj.invoke) {
                ++me->objectRefs[n];
                *i = n;
                return Object_OK;
            }

            // not the object we wanted, but a full slot, so increment and
            // continue
            ++objectsNum;
            continue;
        }

        // slot is free, if the first one keep track of it
        if (firstFree == SIZE_MAX) {
            firstFree = n;
        }

        // if we have an empty slot and we checked all objects, get out and
        // assign
        if ((objectsNum == me->objectsNum) && (firstFree != SIZE_MAX)) {
            break;
        }
    }

    if (firstFree != SIZE_MAX) {
        me->objectRefs[firstFree] = 1;
        me->objects[firstFree] = obj;
        ++me->objectsNum;
        Object_retain(obj);
        *i = firstFree;
        return Object_OK;
    }

    return Object_ERROR_NOSLOTS;
}

// Return the kernel object to which an outbound object forwards invokes.
// If there is no object at that slot, return Object_NULL.  Otherwise, the
// returned object has been retained, and the caller is repsonsible for
// releasing it.
//
static inline Object ObjectTable_recoverObject(ObjectTable *me, size_t h)
{
    if (h < me->objectsLen) {
        Object o = me->objects[h];
        if (!Object_isNull(o)) {
            Object_retain(o);
            return o;
        }
    }

    return Object_NULL;
}

// Returns the current reference count for an object in the object table. If
// no object exists at the given location then an error is returned.
static inline int ObjectTable_queryRefCount(ObjectTable *me, size_t h, int32_t *refCount)
{
    if (h < me->objectsLen) {
        Object o = me->objects[h];
        if (!Object_isNull(o)) {
            *refCount = me->objectRefs[h];
            return Object_OK;
        }
    }

    return Object_ERROR;
}

// Empty the object table entry and release the object.
//
static inline void ObjectTable_closeHandle(ObjectTable *me, size_t h)
{
    if (!Object_isNull(me->objects[h])) {
        Object_release(me->objects[h]);
        me->objects[h].invoke = NULL;
        --me->objectsNum;
    }
}

// Increment the count in the references table.
//
static inline void ObjectTable_retainHandle(ObjectTable *me, size_t h)
{
    if (h < me->objectsLen) {
        ++me->objectRefs[h];
    }
}

// Decrement the count in the references table, and release the associated
// object when it reaches zero.
//
static inline void ObjectTable_releaseHandle(ObjectTable *me, size_t h)
{
    if (h < me->objectsLen) {
        int ref = --me->objectRefs[h];
        if (ref == 0) {
            ObjectTable_closeHandle(me, h);
        }
    }
}

static inline void ObjectTable_destruct(ObjectTable *me)
{
    for (size_t h = 0; h < me->objectsLen; ++h) {
        ObjectTable_closeHandle(me, h);
    }

    HEAP_FREE_PTR(me->objects);
    HEAP_FREE_PTR(me->objectRefs);
    me->objectsLen = 0;
}

static inline int ObjectTable_construct(ObjectTable *me, uint32_t size)
{
    me->objects = HEAP_ZALLOC_ARRAY(Object, size);
    me->objectRefs = HEAP_ZALLOC_ARRAY(int32_t, size);
    me->objectsNum = 0;
    if (me->objects == NULL || me->objectRefs == NULL) {
        if (me->objects) {
            HEAP_FREE_PTR(me->objects);
        }

        if (me->objectRefs) {
            HEAP_FREE_PTR(me->objectRefs);
        }

        me->objectsLen = 0;
        return Object_ERROR;
    }

    me->objectsLen = size;
    return Object_OK;
}

#endif  // __OBJECTTABLE_H
