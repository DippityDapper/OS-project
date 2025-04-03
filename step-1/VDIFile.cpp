#include <fcntl.h>
#include <cstdio>
#include <iostream>
#include "VDIFile.h"

bool VDIFile::Open(char *fn)
{
    fileDescriptor = open(fn, O_RDWR);

    if (fileDescriptor < 0)
    {
        std::cerr << "Could not open file " << fn << "\n";
        return false;
    }

    cursor = 0;
    header = new VDIHeader;

    ssize_t bytesRead = read(fileDescriptor, header, sizeof(VDIHeader));
    if (bytesRead != sizeof(VDIHeader))
    {
        std::cerr << "Could not read from file " << fn << "\n";
        close(fileDescriptor);
        delete header;
        header = nullptr;
        return false;
    }

    if (header->imageType == 1)
    {
        uint32_t numBlocks = header->blocksInHDD;
        translationMap = new uint32_t[numBlocks];

        lseek(fileDescriptor, header->offsetBlocks, SEEK_SET);
        read(fileDescriptor, translationMap, numBlocks * sizeof(uint32_t));
    }
    else
    {
        translationMap = nullptr;
    }

    return true;
}

void VDIFile::Close()
{
    if (fileDescriptor >= 0)
    {
        close(fileDescriptor);
        fileDescriptor = -1;
    }
    delete header;
    header = nullptr;
    if (translationMap)
    {
        delete[] translationMap;
        translationMap = nullptr;
    }
}

ssize_t VDIFile::Read(void *buf, size_t count)
{
    size_t totalRead = 0;
    uint8_t *buffer = reinterpret_cast<uint8_t*>(buf);

    while (count > 0)
    {
        uint32_t blockSize = header->blockSize;
        uint32_t logicalBlock = cursor / blockSize;
        uint32_t offsetInBlock = cursor % blockSize;

        size_t bytesInBlock = blockSize - offsetInBlock;
        if (bytesInBlock > count)
            bytesInBlock = count;

        uint32_t physicalBlock = 0;
        if (translationMap)
            physicalBlock = translationMap[logicalBlock];

        if (physicalBlock == 0xFFFFFFFF)
        {
            memset(buffer, 0, bytesInBlock);
        }
        else if (physicalBlock == 0xFFFFFFFE)
        {
            memset(buffer, 0, bytesInBlock);
        }
        else
        {
            off_t physicalOffset;

            if (translationMap)
                physicalOffset = header->offsetData + (physicalBlock * blockSize) + offsetInBlock;
            else
                physicalOffset = header->offsetData + (logicalBlock * blockSize) + offsetInBlock;

            lseek(fileDescriptor, physicalOffset, SEEK_SET);
            ssize_t bytesRead = read(fileDescriptor, buffer, bytesInBlock);
            if (bytesRead < 0)
            {
                std::cerr << "Could not read from file descriptor " << fileDescriptor << "\n";
                return -1;
            }
        }

        cursor += bytesInBlock;
        buffer += bytesInBlock;
        totalRead += bytesInBlock;
        count -= bytesInBlock;
    }
    return totalRead;
}

ssize_t VDIFile::Write(void *buf, size_t count)
{
    ssize_t totalWritten = 0;
    uint8_t* buffer = reinterpret_cast<uint8_t*>(buf);

    while (count > 0)
    {
        uint32_t blockSize = header->blockSize;
        uint32_t logicalBlock = cursor / blockSize;
        uint32_t offsetInBlock = cursor % blockSize;

        size_t bytesInBlock = blockSize - offsetInBlock;
        if (bytesInBlock > count)
            bytesInBlock = count;

        uint32_t physicalBlock = 0;
        if (translationMap)
            physicalBlock = translationMap[logicalBlock];

        if (physicalBlock == 0xFFFFFFFF)
        {
            physicalBlock = header->blocksAllocated;
            header->blocksAllocated++;
            translationMap[logicalBlock] = physicalBlock;

            lseek(fileDescriptor, header->offsetBlocks + (logicalBlock * sizeof(uint32_t)), SEEK_SET);
            write(fileDescriptor, &translationMap[logicalBlock], sizeof(uint32_t));

            lseek(fileDescriptor, 0, SEEK_SET);
            write(fileDescriptor, header, sizeof(VDIHeader));

            off_t newBlockOffset = header->offsetData + (physicalBlock * blockSize);
            lseek(fileDescriptor, newBlockOffset, SEEK_SET);

            uint8_t* zeros = new uint8_t[blockSize];
            memset(zeros, 0, blockSize);
            write(fileDescriptor, zeros, blockSize);
            delete[] zeros;
        }
        off_t physicalOffset = header->offsetData + (physicalBlock * blockSize) + offsetInBlock;
        lseek(fileDescriptor, physicalOffset, SEEK_SET);

        ssize_t bytesWritten = write(fileDescriptor, buffer, bytesInBlock);

        if (bytesWritten < 0)
        {
            std::cerr << "Could not write to file descriptor " << fileDescriptor << "\n";
            return -1;
        }

        cursor += bytesWritten;
        buffer += bytesWritten;
        totalWritten += bytesWritten;
        count -= bytesWritten;
    }
    return totalWritten;
}

uint32_t VDIFile::lSeek(uint32_t offset, int anchor)
{
    uint64_t newCursor = 0;
    if (anchor == SEEK_SET_)
        newCursor = offset;
    else if (anchor == SEEK_CUR_)
        newCursor = offset + cursor;
    else if (anchor == SEEK_END_)
        newCursor = header->diskSize - offset;
    else
    {
        std::cerr << "Invalid anchor type " << anchor << "\n";
        return cursor;
    }

    if (newCursor > header->diskSize)
    {
        std::cerr << "Cursor exceeded disk" << "\n";
        return cursor;
    }

    cursor = newCursor;
    return cursor;
}
