// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <linux/elf.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include "DmaMemPool.h"
#include "MinkDaemon_logging.h"
#include "TaImageReader.h"
#include "memscpy.h"

using namespace std;

/**
 * Create TA Buffer from TA Image file
 *
 * param[in]        searchPaths                TEnv Object from HLOS Mink Daemon
 * param[in]        uuid                       uuid for TA to load
 * param[out]       imageObj                   TA image Object
 *
 * return Object_OK if successful
 */
taImageStatus TAImageReader::createTAImageReader(std::list<std::string> searchPaths,
                                                 std::string uuid, TAImageReader **imageObj)
{
    taImageStatus ret = taImageStatus::kErrImageNotFound;
    *imageObj = new TAImageReader(searchPaths, uuid);
    ret = (*imageObj)->checkTABufferStatus();
    if (taImageStatus::kErrOk != ret) {
        LOG_ERR("Failed to construct Buffer from TA with uid %s Error Code %d",
                uuid.c_str(), ret);
        delete (*imageObj);
        *imageObj = nullptr;
    }

    return ret;
}

/**
 * Read all Split bins to buffer
 *
 * param[in]        path                       Path to locations
 *
 * return true if successful
 */
bool TAImageReader::ReadSplitBinsToBuf(string &path)
{
    bool isReadTAImage = false;
    bool is64 = false;
    size_t phdrTableOffset = 0, binOffset = 0, bufferOffset = 0;
    fstream splitbin;
    int32_t file_size = 0;
    vector<uint8_t> imageBuffer;
    vector<size_t> offset;
    Elf64_Phdr phdr64;
    Elf32_Phdr phdr32;
    ElfHdr *pElfHdr = nullptr;

    // for each of the remaining segments, read into buffer at offset[seg]
    size_t pathLen = path.length();
    LOG_MSG("Path for b00 %s", path.c_str());

    // open b00 file as inputFile
    ifstream mImageFile(path, mImageFile.in | mImageFile.binary | mImageFile.ate);
    if (!mImageFile.is_open()) {
        LOG_ERR("Failed to open b00 File");
        goto exit;
    }

    file_size = (int32_t)mImageFile.tellg();
    if (file_size <= 0) {
        LOG_ERR("Invalid b00 Size");
        goto exit;
    }

    // Reset to beg
    mImageFile.seekg(0, mImageFile.beg);
    LOG_MSG("ReadSplitBinsToBuf file_size = %d", file_size);
    imageBuffer.reserve(file_size);
    imageBuffer.assign((istreambuf_iterator<char>(mImageFile)), istreambuf_iterator<char>());

    pElfHdr = (ElfHdr *)(void *)(imageBuffer.data());
    if (nullptr == pElfHdr) {
        LOG_ERR("Invalid TA image buffer");
        goto exit;
    }
    offset.resize(pElfHdr->Elf64.e_phnum);

    // Examine buffer's contents as an elf file, check segment count
    // and get offsets of remaining segments
    if (imageBuffer.size() < sizeof(ElfHdr)) {
        LOG_ERR("ReadSplitBinsToBuf buffer->size()=%zu", imageBuffer.size());
        goto exit;
    }

    LOG_MSG("Number of Program header %d", pElfHdr->Elf64.e_phnum);

    switch (pElfHdr->Elf32.e_ident[EI_CLASS]) {
        case ELFCLASS32:
            is64 = false;
            break;
        case ELFCLASS64:
            is64 = true;
            break;
        default:
            LOG_ERR("Unknown File Type");
            return false;
    }

    if (is64) {
        phdrTableOffset = (size_t)pElfHdr->Elf64.e_phoff;
        LOG_MSG("ReadSplitBinsToBuf phdrTableOffset=%zu", phdrTableOffset);
        for (size_t phi = 1; phi < pElfHdr->Elf64.e_phnum; ++phi) {
            bufferOffset = phdrTableOffset + phi * sizeof(Elf64_Phdr);
            memscpy(&phdr64, sizeof(Elf64_Phdr), imageBuffer.data() + bufferOffset,
                    imageBuffer.size() - bufferOffset);
            offset[phi] = (size_t)phdr64.p_offset;
            LOG_MSG("ReadSplitBinsToBuf offset[%zu]=%zu", phi, offset[phi]);
        }
    } else {
        phdrTableOffset = pElfHdr->Elf32.e_phoff;
        LOG_MSG("ReadSplitBinsToBuf phdrTableOffset=%zu", phdrTableOffset);
        for (size_t phi = 1; phi < pElfHdr->Elf64.e_phnum; ++phi) {
            bufferOffset = phdrTableOffset + phi * sizeof(Elf32_Phdr);
            memscpy(&phdr32, sizeof(Elf32_Phdr), imageBuffer.data() + bufferOffset,
                    imageBuffer.size() - bufferOffset);
            offset[phi] = (size_t)phdr32.p_offset;
            LOG_MSG("ReadSplitBinsToBuf offset[%zu]=%zu", phi, offset[phi]);
        }
    }

    // Iterate all Program headers
    for (size_t programSegments = 1; programSegments < pElfHdr->Elf64.e_phnum; ++programSegments) {
        path[pathLen - 1] = (char)('0' + programSegments);
        LOG_MSG("ReadSplitBinsToBuf path=%s", path.c_str());
        // Read segment into buffer starting at offset[i]
        splitbin.open(path.c_str(), splitbin.in | splitbin.binary | splitbin.ate);
        LOG_MSG("Opening File");
        if (splitbin.is_open()) {
            file_size = (int32_t)splitbin.tellg();
            LOG_MSG("Flie Stream Size %d", file_size);
            // Reset ifstream to beginning of the file
            splitbin.seekg(0, splitbin.beg);
            // Copy file's contents into buffer at specified offset
            binOffset = offset[programSegments];
            if ((file_size + binOffset) > imageBuffer.size()) {
                imageBuffer.resize(file_size + binOffset);
                pElfHdr = (ElfHdr *)(void *)(imageBuffer.data());
                LOG_MSG("Total Buffer Size %d", imageBuffer.size());
            }
            std::copy((istreambuf_iterator<char>(splitbin)), istreambuf_iterator<char>(),
                      &(imageBuffer[binOffset]));
            splitbin.close();
        } else {
            LOG_MSG("Unable to Open split bin %s File", path.c_str());
            goto exit;
        }
    }

    // Create DMA Buffer
    if (taImageStatus::kBuffAllocated != createImageBuffer(&imageBuffer)) {
        LOG_ERR("Failed to Allocate Buffer");
        isReadTAImage = false;
        goto exit;
    }
    isReadTAImage = true;

exit:
    imageBuffer.clear();
    // Shrink memory to zero
    imageBuffer.shrink_to_fit();
    offset.clear();
    mImageFile.close();

    return isReadTAImage;
}

/**
 * Get MBN format file image
 *
 * param[in]        imagePath                       Path of TA mbn format file image
 * param[in]        fileSize                        The length of TA mbn format file image
 *
 * return true if successful
 */
bool TAImageReader::getImageMbnFile(string imagePath, size_t fileSize)
{
    bool isGetTAImage = false;

    LOG_MSG("Opening %s", imagePath.c_str());
    // Check if File was found and open was successfull
    mImageFile.open(imagePath.c_str(), mImageFile.in | mImageFile.ate | mImageFile.binary);
    if (false == mImageFile.is_open()) {
        LOG_MSG("File cannot be opened");
        goto exit;
    }

    LOG_MSG("File Size %zu", fileSize);
    mImageFile.seekg(0, mImageFile.beg);
    if (taImageStatus::kBuffAllocated != createImageBuffer(fileSize)) {
        LOG_ERR("Failed to Allocate Buffer");
        goto exit;
    }
    isGetTAImage = true;

exit:
    mImageFile.close();

    return isGetTAImage;
}

taImageStatus TAImageReader::createImageBuffer(vector<uint8_t> *rawData)
{
    // Create DMA Buffer
    if (MEM_OP_SUCCESS != DMAMemPoolGetBuff(mMemBuffer, rawData->size())) {
        LOG_ERR("Failed to allocated DMA Buff Heap Memeory");
        mBufferStatus = taImageStatus::kErrBuffAllocateFailed;
        goto exit;
    }

    // CPU Access Start for Syncing DMA Buffer
    if (MEM_OP_SUCCESS != DMAMemPoolSync(mMemBuffer, true)) {
        LOG_ERR("Failed to start sync DMA Buff Heap Memeory");
        mBufferStatus = taImageStatus::kErrBuffAllocateFailed;
        goto exit;
    }

    memscpy(mMemBuffer->memBuf, mMemBuffer->bufferLen, rawData->data(), rawData->size());
    LOG_MSG("Splitbin Size %d", rawData->size());

    // CPU Access Stop for Syncing DMA Buffer
    if (MEM_OP_SUCCESS != DMAMemPoolSync(mMemBuffer, false)) {
        LOG_ERR("Failed to stop sync DMA Buff Heap Memeory");
        mBufferStatus = taImageStatus::kErrBuffAllocateFailed;
        goto exit;
    }

    mBufferStatus = taImageStatus::kBuffAllocated;

exit:
    return mBufferStatus;
}

taImageStatus TAImageReader::createImageBuffer(size_t buffLen)
{
    // Buffer Allocated copy image data into Buffer
    if (MEM_OP_SUCCESS != DMAMemPoolGetBuff(mMemBuffer, buffLen)) {
        LOG_ERR("Failed to allocated DMA Buff Heap Memeory");
        mBufferStatus = taImageStatus::kErrBuffAllocateFailed;
        goto exit;
    }

    // CPU Access Start for Syncing DMA Buffer
    if (MEM_OP_SUCCESS != DMAMemPoolSync(mMemBuffer, true)) {
        LOG_ERR("Failed to start sync DMA Buff Heap Memeory");
        mBufferStatus = taImageStatus::kErrBuffAllocateFailed;
        goto exit;
    }

    // Memory Allocated
    mImageFile.read((char *)mMemBuffer->memBuf, mMemBuffer->bufferLen);

    // CPU Access Stop for Syncing DMA Buffer
    if (MEM_OP_SUCCESS != DMAMemPoolSync(mMemBuffer, false)) {
        LOG_ERR("Failed to stop sync DMA Buff Heap Memeory");
        mBufferStatus = taImageStatus::kErrBuffAllocateFailed;
        goto exit;
    }

    mBufferStatus = taImageStatus::kBuffAllocated;

exit:
    return mBufferStatus;
}

TAImageReader::TAImageReader(std::list<std::string> searchPaths, std::string uuid)
    : mBufferStatus(taImageStatus::kErrImageNotFound), mMemBuffer(nullptr)
{
    bool imageFound = false;
    string pathName = "";

    if (searchPaths.empty()) {
        // Return Image not Found
        LOG_MSG("Empty TA Path list");
        return;
    }
    mMemBuffer = new MemoryBuffer();

    LOG_MSG("Searching Image File");
    for (list<std::string>::iterator it = searchPaths.begin(); it != searchPaths.end(); it++) {
        pathName = *it + uuid + string(".mbn");
        struct stat buffer;
        if (0 != stat(pathName.c_str(), &buffer)) {
            LOG_MSG("%s.mbn File not found @ %s", uuid.c_str(), pathName.c_str());
            pathName = *it + uuid + string(".b00");
            if (0 != stat(pathName.c_str(), &buffer)) {
                LOG_MSG("%s.b00 also File not found @ %s", uuid.c_str(), pathName.c_str());
            } else {
                imageFound = ReadSplitBinsToBuf(pathName);
                if (true == imageFound) {
                    if (taImageStatus::kErrBuffAllocateFailed == mBufferStatus) {
                        LOG_ERR("Splitbin Buffer Allocation Failed for Image");
                    } else {
                        mBufferStatus = taImageStatus::kErrOk;
                    }
                    break;
                }
            }
        } else {
            imageFound = getImageMbnFile(pathName, buffer.st_size);
            if (true == imageFound) {
                if (taImageStatus::kErrBuffAllocateFailed == mBufferStatus) {
                    LOG_ERR("Mbn Buffer Allocation Failed for Image");
                } else {
                    mBufferStatus = taImageStatus::kErrOk;
                }
                break;
            }
        }
    }
}

TAImageReader::~TAImageReader()
{
    if (nullptr != mMemBuffer) {
        LOG_MSG("Release buffer = %p", mMemBuffer->memBuf);
        if (MEM_OP_SUCCESS != DMAMemPoolReleaseBuff(mMemBuffer)) {
            LOG_ERR("Failed to release Buffer");
        }
    } else {
        LOG_MSG("Nothing to Do: DMA Buffer not Allocated");
    }
}
