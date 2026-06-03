// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TAIMAGEREADER_H
#define __TAIMAGEREADER_H

#include <linux/elf.h>
#include <sys/mman.h>
#include <cstring>
#include <fstream>
#include <list>
#include <string>
#include "DmaMemPool.h"

enum class taImageStatus: int32_t {
    kErrOk = 0,
    kErrInternal,
    kErrBuffAllocateFailed,
    kErrImageNotFound,
    kErrBuffReleaseFailed,
    kBuffAllocated,
};

//@Elf Header Format
typedef union {
    Elf32_Ehdr Elf32;
    Elf64_Ehdr Elf64;
} ElfHdr;

class TAImageReader
{
   private:
    std::fstream mImageFile;
    MemoryBuffer *mMemBuffer;
    taImageStatus mBufferStatus;

    //@internal methods
    bool getImageMbnFile(std::string path, size_t fileSize);
    bool ReadSplitBinsToBuf(std::string &path);
    taImageStatus createImageBuffer(size_t buffLen);
    TAImageReader(std::list<std::string> searchPaths, std::string uuid);

    //@override
    taImageStatus createImageBuffer(std::vector<uint8_t> *rawData);

   public:
    int32_t getFileDescriptor()
    {
        return mMemBuffer->fileDescriptor;
    };
    taImageStatus checkTABufferStatus()
    {
        return mBufferStatus;
    };
    static taImageStatus createTAImageReader(std::list<std::string> searchPaths, std::string uuid,
                                             TAImageReader **imageObj);

    // delete default and copy constructors
    TAImageReader() = delete;
    TAImageReader(const TAImageReader &) = delete;
    ~TAImageReader();
};

#endif // __TAIMAGEREADER_H
