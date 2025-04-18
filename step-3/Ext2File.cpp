#include <iostream>
#include "Ext2File.h"

#define EXT2_SUPERBLOCK_OFFSET 1024
#define EXT2_SUPERBLOCK_SIZE sizeof(SuperBlock)
#define EXT2_SUPER_MAGIC 0xEF53

bool Ext2File::Open(char *fn)
{
    mbrPart = new MBRPartition;
    if (!mbrPart->Open(fn, 0))
    {
        delete mbrPart;
        mbrPart = nullptr;
        return false;
    }

    int partIndex = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (mbrPart->partitionTable[i].partitionType == 0x83)
        {
            partIndex = i;
            break;
        }
    }

    if (partIndex < 0)
    {
        std::cerr << "No ext2 partition of type 0x83 found" << "\n";
        mbrPart->Close();
        delete mbrPart;
        mbrPart = nullptr;
        return false;
    }

    mbrPart->Close();
    if (!mbrPart->Open(fn, partIndex))
    {
        std::cerr << "Failed to Open partition" << partIndex << "\n";
        delete mbrPart;
        mbrPart = nullptr;
        return false;
    }

    if (mbrPart->lSeek(EXT2_SUPERBLOCK_OFFSET, SEEK_SET_) != EXT2_SUPERBLOCK_OFFSET)
    {
        std::cerr << "Failed to read superblocks" << "\n";
        return false;
    }

    superblock = new SuperBlock;
    if (mbrPart->Read(superblock, EXT2_SUPERBLOCK_SIZE) != EXT2_SUPERBLOCK_SIZE)
    {
        std::cerr << "Failed to read superblock" << "\n";
        delete superblock;
        superblock = nullptr;
        return false;
    }

    return true;
}

void Ext2File::Close()
{
    if (mbrPart)
    {
        mbrPart->Close();
        delete mbrPart;
        mbrPart = nullptr;
    }
    if (superblock)
    {
        delete superblock;
        superblock = nullptr;
    }
}

bool Ext2File::FetchBlock(uint32_t blockNum, void *buf)
{
    uint32_t blockSize = 1024 << superblock->logBlockSize;
    uint32_t offset = blockNum * blockSize;

    if (mbrPart->lSeek(offset, SEEK_SET_) != offset)
    {
        std::cerr << "Failed to seek to block offset" << "\n";
        return false;
    }

    ssize_t bytesRead = mbrPart->Read(buf, blockSize);
    if (bytesRead != static_cast<ssize_t>(blockSize))
    {
        std::cerr << "Failed to read. Wrong number of bytes " << bytesRead << "\n";
        return false;
    }

    return true;
}

bool Ext2File::WriteBlock(uint32_t blockNum, void *buf)
{
    uint32_t blockSize = 1024 << superblock->logBlockSize;
    uint32_t offset = blockNum * blockSize;

    if (mbrPart->lSeek(offset, SEEK_SET_) != offset)
    {
        std::cerr << "Failed to seek to block offset" << "\n";
        return false;
    }
    ssize_t bytesWritten = mbrPart->Write(buf, blockSize);

    if (bytesWritten != static_cast<ssize_t>(blockSize))
    {
        std::cerr << "Failed to read. Wrong number of bytes " << bytesWritten << "\n";
        return false;
    }

    return true;
}

bool Ext2File::FetchSuperBlock(uint32_t blockNum, struct SuperBlock *sb)
{
    if (blockNum == 0)
    {
        if (mbrPart->lSeek(EXT2_SUPERBLOCK_OFFSET, SEEK_SET_) != EXT2_SUPERBLOCK_OFFSET)
        {
            std::cerr << "Failed to seek main superblock offset" << "\n";
            return false;
        }
        if (mbrPart->Read(sb, EXT2_SUPERBLOCK_SIZE) != EXT2_SUPERBLOCK_SIZE)
        {
            std::cerr << "Failed to read main superblock" << "\n";
            return false;
        }
    }
    else
    {
        uint32_t blockSize = 1024 << superblock->logBlockSize;
        uint8_t *buf = new uint8_t[blockSize];

        if (!FetchBlock(blockNum, buf))
        {
            std::cerr << "Failed to fetch backup superblock" << "\n";
            return false;
        }
        memcpy(sb, buf, EXT2_SUPERBLOCK_SIZE);
        delete[] buf;
    }

    if (sb->magic != EXT2_SUPER_MAGIC)
    {
        std::cerr << "Invalid superblock magic " << std::hex << sb->magic << "\n";
        return false;
    }

    return true;
}

bool Ext2File::WriteSuperBlock(uint32_t blockNum, struct SuperBlock *sb)
{
    if (blockNum == 0)
    {
        if (mbrPart->lSeek(EXT2_SUPERBLOCK_OFFSET, SEEK_SET_) != EXT2_SUPERBLOCK_OFFSET)
        {
            std::cerr << "Failed to seek main superblock offset" << "\n";
            return false;
        }
        if (mbrPart->Write(sb, EXT2_SUPERBLOCK_SIZE) != EXT2_SUPERBLOCK_SIZE)
        {
            std::cerr << "Failed to write main superblock" << "\n";
            return false;
        }
    }
    else
    {
        uint32_t blockSize = 1024 << superblock->logBlockSize;
        uint8_t *buf = new uint8_t[blockSize];

        memcpy(buf, sb, EXT2_SUPERBLOCK_SIZE);
        if (!WriteBlock(blockNum, buf))
        {
            std::cerr << "Failed to fetch backup superblock" << "\n";
            return false;
        }
        delete[] buf;
    }

    return true;
}

bool Ext2File::FetchBGDT(uint32_t blockNum, BlockGroupDescriptor *bgdt)
{
    uint32_t blockSize = 1024 << superblock->logBlockSize;
    uint32_t totalBG = (superblock->blocksCount + superblock->blocksPerGroup - 1) / superblock->blocksPerGroup;
    uint32_t totalBytes = totalBG * sizeof(BlockGroupDescriptor);
    uint32_t blocksNeeded = (totalBytes + blockSize - 1) / blockSize;

    uint8_t *tmp = new uint8_t[blocksNeeded * blockSize];

    for (uint32_t i = 0; i < blocksNeeded; i++)
    {
        if (!FetchBlock(blockNum + i, tmp + i * blockSize))
        {
            std::cerr << "FetchBGDT failed to fetch block " << (blockNum + i) << "\n";
            delete[] tmp;
            return false;
        }
    }

    memcpy(bgdt, tmp, totalBytes);
    delete[] tmp;
    return true;
}

bool Ext2File::WriteBGDT(uint32_t blockNum, BlockGroupDescriptor *bgdt)
{
    uint32_t blockSize = 1024 << superblock->logBlockSize;
    uint32_t totalBG = (superblock->blocksCount + superblock->blocksPerGroup - 1) / superblock->blocksPerGroup;
    uint32_t totalBytes = totalBG * sizeof(BlockGroupDescriptor);
    uint32_t blocksNeeded = (totalBytes + blockSize - 1) / blockSize;

    uint8_t *tmp = new uint8_t[blocksNeeded * blockSize];

    memcpy(tmp, bgdt, totalBytes);
    for (uint32_t i = 0; i < blocksNeeded; i++)
    {
        if (!WriteBlock(blockNum + i, tmp + i * blockSize))
        {
            std::cerr << "FetchBGDT failed to write block " << (blockNum + i) << "\n";
            delete[] tmp;
            return false;
        }
    }

    delete[] tmp;
    return true;
}
