// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <cstdbool>
#include <map>
#include <memory>
#include <sys/mman.h>
#include "DmaMemPool.h"
#include "MinkDaemon_logging.h"
#include "TaImageReader.h"

// HEAP ID Allocted for TA Loading
static const int32_t ION_QSEE_TA_HEAP_ID(19);

// HEAP Names
static const char *DMA_BUF_HEAP("qcom,qseecom-ta");
static const char *ION_BUF_HEAP("qsecom-ta");

MemoryBuffer::MemoryBuffer() : fileDescriptor(INVALID_FD), memBuf(nullptr)
{
    dmaBufferAllocator = new BufferAllocator();
    bufferLen = 0U;
}

MemoryBuffer::~MemoryBuffer()
{
    allocatorInit = false;
    if (INVALID_FD != fileDescriptor) {
        close(fileDescriptor);
        fileDescriptor = INVALID_FD;
    }

    if (nullptr != dmaBufferAllocator) {
        delete (dmaBufferAllocator);
        dmaBufferAllocator = nullptr;
    }

    if (nullptr != memBuf) {
        munmap(memBuf, bufferLen);
    }
}

/**
 * Init DMA Buffer.
 *
 * param[in]   memoryPtr         the pointer of dma memory
 *
 * return true if successful
 */
bool DMAMemPoolInit(struct MemoryBuffer *memoryPtr)
{
    bool isDMAInit = false;
    int32_t ret = INVALID_FD;
    uint32_t flags = ION_FLAG_CACHED;

    // Already Initialized
    if (true == memoryPtr->allocatorInit) {
        isDMAInit = true;
        goto exit;
    }

    LOG_MSG("DMA Init");
    ret =
        MapDmabufHeapNameToIonHeap(memoryPtr->dmaBufferAllocator, DMA_BUF_HEAP, ION_BUF_HEAP, flags,
                                   ION_HEAP(ION_QSEE_TA_HEAP_ID), flags);  // dmabuf heap flag
    if (ret < 0) {
        LOG_ERR("Failed to Map Heap Mem %d", ret);
        goto exit;
    }

    memoryPtr->allocatorInit = true;
    isDMAInit = true;

exit:
    return isDMAInit;
}

/**
 * Get DMA Buffer from DMABuffMemPool.
 *
 * param[in]   memoryPtr         the pointer of dma buffer memory
 * param[in]   buffLen           the length of dma buffer memory
 *
 * return MEM_OP_SUCCESS if successful
 */
int32_t DMAMemPoolGetBuff(struct MemoryBuffer *memoryPtr, size_t buffLen)
{
    int32_t ret = MEM_ALLOC_FAILED;

    if (false == DMAMemPoolInit(memoryPtr)) {
        LOG_MSG("DMA Allocator not initialized");
        goto exit;
    }

    memoryPtr->bufferLen = ALIGN_PAGESIZE(buffLen);
    memoryPtr->fileDescriptor =
        DmabufHeapAlloc(memoryPtr->dmaBufferAllocator, DMA_BUF_HEAP, memoryPtr->bufferLen, 0, 0);
    if (memoryPtr->fileDescriptor < 0) {
        LOG_ERR("Failed to Allocate Buffer %d", memoryPtr->fileDescriptor);
        goto exit;
    }

    memoryPtr->memBuf = (unsigned char *)mmap(NULL, memoryPtr->bufferLen, PROT_READ | PROT_WRITE,
                                              MAP_SHARED, memoryPtr->fileDescriptor, 0);
    if (memoryPtr->memBuf == MAP_FAILED) {
        LOG_ERR("Failed to Allocate Buffer");
        ret = MEM_MMAP_FAILED;
        goto exit;
    }

    LOG_MSG("Mmaped heap successful, memBuf = %p and Len = %d", memoryPtr->memBuf,
            memoryPtr->bufferLen);

    return MEM_OP_SUCCESS;

exit:
    memoryPtr->memBuf = (unsigned char *)MAP_FAILED;

    return ret;
}

/**
 * Perform Sync Operation for buffer.
 *
 * param[in]   memoryPtr         the pointer of dma buffer memory
 * param[in]   SyncStart         sync operation type
 *
 * return MEM_OP_SUCCESS if successful
 */
int32_t DMAMemPoolSync(struct MemoryBuffer *memoryPtr, bool SyncStart)
{
    int32_t retVal = MEM_SYNC_FAILED, ret = INVALID_FD;

    if (false == memoryPtr->allocatorInit) {
        LOG_MSG("DMA Allocator not initialized");
        goto exit;
    }

    // Check Operation Type
    if (true == SyncStart) {
#ifdef DMA_BUF2_ENABLE
        ret = DmabufHeapCpuSyncStart2(memoryPtr->dmaBufferAllocator, memoryPtr->fileDescriptor,
                                     kSyncWrite);
#else
        ret = DmabufHeapCpuSyncStart(memoryPtr->dmaBufferAllocator, memoryPtr->fileDescriptor,
                                     kSyncWrite, NULL, NULL);
#endif
        if (ret < 0) {
            LOG_ERR("Failed to CPU Start Sync Operation");
            goto exit;
        }
    } else {
#ifdef DMA_BUF2_ENABLE
       ret = DmabufHeapCpuSyncEnd2(memoryPtr->dmaBufferAllocator, memoryPtr->fileDescriptor,
                                   kSyncWrite);
#else
       ret = DmabufHeapCpuSyncEnd(memoryPtr->dmaBufferAllocator, memoryPtr->fileDescriptor,
                                   kSyncWrite, NULL, NULL);
#endif
        if (ret < 0) {
            LOG_ERR("Failed to CPU Start Sync Operation");
            goto exit;
        }
    }

    LOG_MSG("Mmaped Heap memBuf = %p Synced", memoryPtr->memBuf);

    retVal = MEM_OP_SUCCESS;

exit:
    return retVal;
}

/**
 * Release DMA MemPool Buffer.
 *
 * param[in]   memoryPtr         the pointer of dma buffer memory
 *
 * return MEM_OP_SUCCESS if successful
 */
int32_t DMAMemPoolReleaseBuff(struct MemoryBuffer *memoryPtr)
{
    int32_t retVal = MEM_RELEASE_FAILED, ret = INVALID_FD;

    if (nullptr == memoryPtr) {
        LOG_ERR("Invailed the pointer of dma buffer memory");
        goto exit;
    }

    if ((false == memoryPtr->allocatorInit) || (MAP_FAILED == memoryPtr->memBuf)) {
        LOG_ERR("DMA Allocator not initialized or mapped");
        goto exit;
    }

    // Do a precautius End
#ifdef DMA_BUF2_ENABLE
    ret = DmabufHeapCpuSyncEnd2(memoryPtr->dmaBufferAllocator, memoryPtr->fileDescriptor, kSyncWrite);
#else
    ret = DmabufHeapCpuSyncEnd(memoryPtr->dmaBufferAllocator, memoryPtr->fileDescriptor, kSyncWrite,
                               NULL, NULL);
#endif
    if (ret < 0) {
        LOG_ERR("Failed to CPU Start Sync Operation");
        goto exit;
    }

    // Unmap the mapped DMA Memory
    ret = munmap(memoryPtr->memBuf, memoryPtr->bufferLen);
    if (ret < 0) {
        LOG_ERR("Error unmap Failed %d", ret);
        goto exit;
    }

    memoryPtr->bufferLen = 0;
    LOG_MSG("Mmaped Heap @ %p Released", memoryPtr->memBuf);

    retVal = MEM_OP_SUCCESS;

exit:
    if (nullptr != memoryPtr) {
        delete (memoryPtr);
        memoryPtr = nullptr;
    }

    return retVal;
}
