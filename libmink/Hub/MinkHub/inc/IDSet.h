// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __IDSET_H
#define __IDSET_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "MinkHubUtils.h"
#include "object.h"

// Each entry is either:
//  - Available: group == 0, mask == 0
//  - Used: group == groupID, mask == one bit per ID
//
typedef struct {
    uint32_t group;
    uint32_t mask;
} IDSetEntry;

#define IDSet_GROUP(id) ((id) | UINT32_C(31))
#define IDSet_MASK(id) (UINT32_C(1) << (31 & (id)))

typedef struct IDSet IDSet;

struct IDSet {
    IDSetEntry entries[2];  // TODO : why is this a 2-element array?
    IDSet *next;
};

// Construct from zero-initialized memory
//
static inline void IDSet_zconstruct(IDSet *set)
{
    // nothing to do
    (void)set;
}

static inline void IDSet_empty(IDSet *set)
{
    IDSet *pa, *pb;

    for (pa = set->next; pa; pa = pb) {
        pb = pa->next;
        free(pa);
    }

    set->next = NULL;
    memset(set->entries, 0, sizeof(set->entries));
}

// This actually doesnt destroy the IDset but instead empties it
static inline void IDSet_destruct(IDSet *set)
{
    IDSet_empty(set);
}

static inline bool IDSet_testOrSet(IDSet *set, uint32_t id, bool modifySet)
{
    uint32_t group = IDSet_GROUP(id);
    uint32_t mask = IDSet_MASK(id);
    size_t ndx;

    IDSetEntry insert = {
        .group = group,
        .mask = mask,
    };

    do {
        // Examine entries in this IDSet structure
        // We maintain a sorted (by group) list for efficiency reasons (ex.subset compares)
        for (ndx = 0; ndx < C_LENGTHOF(set->entries); ++ndx) {
            IDSetEntry *pe = set->entries + ndx;

            if (pe->group > 0 && pe->group < group) {
                continue;
            }

            if (pe->group == group) {
                // matching group, test/set id bit
                if (modifySet) {
                    pe->mask |= mask;
                }
                return 0 != (pe->mask & mask);
            }

            if (!modifySet) {
                return false;
            }

            // group not present, insert entry and push data forward
            IDSetEntry tmp = *pe;

            *pe = insert;
            insert = tmp;

            // End of the line, return true since we reached this only on insert scenarios
            if (insert.group == 0) {
                return true;
            }
        }

        // Get/create the next IDSet
        if (!set->next && modifySet) {
            set->next = (IDSet *)calloc(1, sizeof(IDSet));
        }
        set = set->next;
    } while (set);

    return false;
}

static inline int32_t IDSet_set(IDSet *set, uint32_t id)
{
    int32_t ret = Object_OK;
    ret = (true == IDSet_testOrSet(set, id, true)) ? Object_OK : Object_ERROR_MEM;
    return ret;
}

static inline bool IDSet_test(IDSet *set, uint32_t id)
{
    return IDSet_testOrSet(set, id, false);
}

static inline bool IDSet_clear(IDSet *set, uint32_t id)
{
    uint32_t group = IDSet_GROUP(id);
    uint32_t mask = IDSet_MASK(id);
    size_t ndx;

    do {
        // Examine entries in this IDSet structure
        for (ndx = 0; ndx < C_LENGTHOF(set->entries); ++ndx) {
            IDSetEntry *pe = set->entries + ndx;
            if (pe->group == group) {
                // matching used entry
                pe->mask &= ~mask;
                return true;
            }
        }

        set = set->next;
    } while (set);

    return false;
}

static inline bool IDSet_isSubSet(IDSet *set, IDSet *subset)
{
    // Loop over subset and tries to find group/mask in the set.
    do {
        size_t subndx;
        for (subndx = 0; subndx < C_LENGTHOF(subset->entries); ++subndx) {
            IDSetEntry *sub_entry = subset->entries + subndx;
            IDSetEntry *set_entry = NULL;
            bool found = false;

            // loop over set until group matches or isnt found
            do {
                size_t setndx;
                for (setndx = 0; setndx < C_LENGTHOF(set->entries); ++setndx) {
                    set_entry = set->entries + setndx;

                    if (set_entry->group < sub_entry->group) {
                        continue;
                    }

                    if (set_entry->group == sub_entry->group) {
                        // Check mask for subset
                        if ((sub_entry->mask & set_entry->mask) != sub_entry->mask) {
                            return false;
                        }

                        found = true;
                        break;
                    }

                    return false;
                }

                if (found) {
                    break;
                }

                set = set->next;
            } while (set);
        }

        subset = subset->next;
    } while (set && subset);

    // If we reached end of the subset then all entries exists in set
    if (!subset) {
        return true;
    }

    return false;
}

static inline int32_t IDSet_dup(IDSet const *set, IDSet *dup)
{
    IDSet id = {0};
    IDSet *tmp = &id;
    int32_t ret = Object_OK;

    memcpy(tmp->entries, set->entries, sizeof(set->entries));

    while (set->next) {
        tmp->next = (IDSet *)calloc(1, sizeof(IDSet));
        MINKHUB_CHECK_ERR(tmp->next != NULL, Object_ERROR_MEM);

        tmp = tmp->next;
        set = set->next;

        memcpy(tmp->entries, set->entries, sizeof(set->entries));
    }

    memcpy(dup, &id, sizeof(IDSet));

exit:
    if (Object_isERROR(ret)) {
        IDSet_empty(&id);
    }
    return ret;
}

#endif  // __IDSET_H
