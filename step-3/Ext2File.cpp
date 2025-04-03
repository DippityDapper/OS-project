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

    superBlock = new SuperBlock;
    if (mbrPart->Read(superBlock, EXT2_SUPERBLOCK_SIZE) != EXT2_SUPERBLOCK_SIZE)
    {
        std::cerr << "Failed to read superblock" << "\n";
        delete superBlock;
        superBlock = nullptr;
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
    if (superBlock)
    {
        delete superBlock;
        superBlock = nullptr;
    }
}

bool Ext2File::FetchBlock(uint32_t blockNum, void *buf)
{
    uint32_t blockSize = 1024 << superBlock->logBlockSize;
    uint32_t offset = (superBlock->firstDataBlock + blockNum) * blockSize;

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
    uint32_t blockSize = 1024 << superBlock->logBlockSize;
    uint32_t offset = (superBlock->firstDataBlock + blockNum) * blockSize;

    if (mbrPart->lSeek(offset, SEEK_SET_) != offset)
    {
        std::cerr << "Failed to seek to block offset" << "\n";
        return false;
    }

    ssize_t bytesWritten = mbrPart->Write(buf, blockSize);
    if (bytesWritten != static_cast<ssize_t>(blockSize))
    {
        std::cerr << "Failed to Write. Wrong number of bytes " << bytesWritten << "\n";
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
        if (!FetchBlock(blockNum, sb))
        {
            std::cerr << "Failed to fetch backup superblock" << "\n";
            return false;
        }
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
        uint32_t blockSize = 1024 << superBlock->logBlockSize;

        uint8_t *blockBuffer = new uint8_t[blockSize];
        if (!FetchBlock(blockNum, blockBuffer))
        {
            std::cerr << "Failed to read block for backup superblock" << "\n";
            delete[] blockBuffer;
            return false;
        }

        memcpy(blockBuffer, sb, EXT2_SUPERBLOCK_SIZE);

        bool result = WriteBlock(blockNum, blockBuffer);
        delete[] blockBuffer;

        if (!result)
        {
            std::cerr << "Failed to write backup superblock" << "\n";
            return false;
        }
    }

    return true;
}

bool Ext2File::FetchBGDT(uint32_t blockNum, BlockGroupDescriptor *bgdt)
{
    uint32_t blockSize = 1024 << superBlock->logBlockSize;
    uint32_t bgdtStartBlock = superBlock->firstDataBlock + 1;
    uint32_t descriptorsPerBlock = blockSize / sizeof(BlockGroupDescriptor);
    uint32_t blockOffset = blockNum / descriptorsPerBlock;
    uint32_t entryOffset = blockNum % descriptorsPerBlock;

    uint8_t *buffer = new uint8_t[blockSize];
    if (!FetchBlock(bgdtStartBlock + blockOffset, buffer)) {
        std::cerr << "FetchBGDT failed to fetch block" << "\n";
        delete[] buffer;
        return false;
    }

    memcpy(bgdt, buffer + (entryOffset * sizeof(BlockGroupDescriptor)),sizeof(BlockGroupDescriptor));

    delete[] buffer;
    return true;
}

bool Ext2File::WriteBGDT(uint32_t blockNum, BlockGroupDescriptor *bgdt)
{
    uint32_t blockSize = 1024 << superBlock->logBlockSize;
    uint32_t bgdtStartBlock = superBlock->firstDataBlock + 1;
    uint32_t descriptorsPerBlock = blockSize / sizeof(BlockGroupDescriptor);
    uint32_t blockOffset = blockNum / descriptorsPerBlock;
    uint32_t entryOffset = blockNum % descriptorsPerBlock;

    uint8_t *buffer = new uint8_t[blockSize];
    if (!FetchBlock(bgdtStartBlock + blockOffset, buffer)) {
        std::cerr << "WriteBGDT failed to fetch current BGDT block" << "\n";
        delete[] buffer;
        return false;
    }

    memcpy(buffer + (entryOffset * sizeof(BlockGroupDescriptor)), bgdt, sizeof(BlockGroupDescriptor));

    bool result = WriteBlock(bgdtStartBlock + blockOffset, buffer);
    if (!result) {
        std::cerr << "WriteBGDT failed to write updated BGDT block" << "\n";
    }

    delete[] buffer;
    return result;
}
