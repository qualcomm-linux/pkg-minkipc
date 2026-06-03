// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __DMAMEMPOOL_H_
#define __DMAMEMPOOL_H_

#include <BufferAllocator/BufferAllocator.h>
#include <BufferAllocator/BufferAllocatorWrapper.h>
#include <ion/ion.h>
#include <object.h>
#include <stddef.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ION_HEAP(bit) bit
#define ALIGN_PAGESIZE(LEN) (((LEN) + (getpagesize() - 1)) & ~((getpagesize() - 1)))

#define startSync true
#define stopSync false

/**
 * Error Codes
 *
 * MEM_OP_SUCCESS:         Success
 * MEM_MMAP_FAILED:        MMAP Failed
 * MEM_ALLOC_FAILED:       DMA Memory Allocation Failed
 * MEM_SYNC_FAILED:        DMA Memory Sync
 * MEM_RELEASE_FAILED:     DMA Memory Released Failed
 */
#define INVALID_FD -1
#define MEM_OP_SUCCESS Object_OK
#define MEM_ERROR_CODE(VAL) (Object_ERROR_USERBASE + (VAL))
#define MEM_MMAP_FAILED MEM_ERROR_CODE(0)
#define MEM_ALLOC_FAILED MEM_ERROR_CODE(1)
#define MEM_SYNC_FAILED MEM_ERROR_CODE(2)
#define MEM_RELEASE_FAILED MEM_ERROR_CODE(3)

/**
 * struct MemoryBuffer
 *
 * allocatorInit:          Init flag in case allocator is already Created
 * fileDescriptor:         file descriptor associated with mmaped Buffer
 * dmaBufferAllocator:     pointer to allocator for buffer.
 * memBuf:                 Memory Buffer
 * bufferLen:              length of Buffer
 *
 */
struct MemoryBuffer {
    bool allocatorInit;
    int fileDescriptor;
    BufferAllocator *dmaBufferAllocator;
    unsigned char *memBuf;
    size_t bufferLen;
    MemoryBuffer();
    ~MemoryBuffer();
};

/**
 * Init DMA Buffer.
 *
 * param[in]   memoryPtr         the pointer of dma memory
 *
 * return true if successful
 */
bool DMAMemPoolInit(struct MemoryBuffer *memoryPtr);

/**
 * Get DMA Buffer from DMABuffMemPool.
 *
 * param[in]   memoryPtr         the pointer of dma buffer memory
 * param[in]   buffLen           the length of dma buffer memory
 *
 * return MEM_OP_SUCCESS if successful
 */
int32_t DMAMemPoolGetBuff(struct MemoryBuffer *memoryPtr, size_t buffLen);

/**
 * Perform Sync Operation for buffer.
 *
 * param[in]   memoryPtr         the pointer of dma buffer memory
 * param[in]   SyncStart         sync operation type
 *
 * return MEM_OP_SUCCESS if successful
 */
int32_t DMAMemPoolSync(struct MemoryBuffer *memoryPtr, bool SyncStart);

/**
 * Release DMA MemPool Buffer.
 *
 * param[in]   memoryPtr         the pointer of dma buffer memory
 *
 * return MEM_OP_SUCCESS if successful
 */
int32_t DMAMemPoolReleaseBuff(struct MemoryBuffer *memoryPtr);

#ifdef __cplusplus
}
#endif

#endif  //__DMAMEMPOOL_H_
