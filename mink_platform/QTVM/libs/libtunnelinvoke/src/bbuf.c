// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "VmOsal.h"
#include "bbuf.h"

//------------------------------------------------------------------------
// BBuf: Bounded Buffer
//------------------------------------------------------------------------

#define BBUF_ALLOC_ALIGNMENT  8


void BBuf_construct(BBuf *me, void *ptr, size_t size)
{
   me->ptr = (char *)ptr;
   me->len = size;
}


void *BBuf_alloc(BBuf *me, size_t size)
{
   char *result = me->ptr;
   size_t len = me->len;
   size_t pad = (uintptr_t) result % BBUF_ALLOC_ALIGNMENT;
   if (pad)
     pad = BBUF_ALLOC_ALIGNMENT - pad;

   if (pad > len || size > len - pad) {
      return NULL;
   }

   me->len = len - pad - size;
   me->ptr = result + pad + size;
   return (void*) (result + pad);
}
