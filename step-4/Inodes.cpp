#include <cstdint>
#include <cstring>
#include "Inodes.h"
#include <iostream>

int32_t Inodes::FetchInode(Ext2File *f, uint32_t iNum, Inode *buf)
{
    if (iNum == 0 || iNum > f->superBlock->inodesCount)
        return -1;

    iNum--;
    uint32_t group = iNum / f->superBlock->inodesPerGroup;
    uint32_t indexInGroup = iNum % f->superBlock->inodesPerGroup;

    uint32_t inodeSize = f->superBlock->inodeSize;
    uint32_t blockSize = 1024 << f->superBlock->logBlockSize;
    uint32_t inodesPerBlock = blockSize / inodeSize;

    uint32_t blockOffset = indexInGroup / inodesPerBlock;
    uint32_t inodeOffset = (indexInGroup % inodesPerBlock) * inodeSize;
    uint32_t blockNum = groupDesc[group].inodeTable + blockOffset;

    uint8_t *block = new uint8_t[blockSize];
    if (f->FetchBlock(blockNum, block) != 0)
    {
        std::cerr << "Error fetching inode block " << blockNum << std::endl;
        return -1;
    }

    std::memcpy(buf, block + inodeOffset, sizeof(Inode));
    delete[] block;
    return 0;
}

int32_t Inodes::WriteInode(Ext2File* f, uint32_t iNum, const Inode* buf)
{
    if (iNum == 0 || iNum > f->superBlock->inodesCount)
        return -1;

    iNum--;
    uint32_t group = iNum / f->superBlock->inodesPerGroup;
    uint32_t indexInGroup = iNum % f->superBlock->inodesPerGroup;

    uint32_t inodeSize = f->superBlock->inodeSize;
    uint32_t blockSize = 1024 << f->superBlock->logBlockSize;
    uint32_t inodesPerBlock = blockSize / inodeSize;

    uint32_t blockOffset = indexInGroup / inodesPerBlock;
    uint32_t inodeOffset = (indexInGroup % inodesPerBlock) * inodeSize;
    uint32_t blockNum = groupDesc[group].inodeTable + blockOffset;

    uint8_t *block = new uint8_t[blockSize];
    if (f->FetchBlock(blockNum, block) != 0)
    {
        std::cerr << "Error fetching inode block " << blockNum << std::endl;
        return -1;
    }

    std::memcpy(block + inodeOffset, buf, sizeof(Inode));

    if (f->WriteBlock(blockNum, block) != 0)
    {
        std::cerr << "Error writing inode block " << blockNum << std::endl;
        return -1;
    }

    delete[] block;
    return 0;
}

int32_t Inodes::InodeInUse(Ext2File* f, uint32_t iNum)
{
    if (iNum == 0 || iNum > f->superBlock->inodesCount)
        return -1;

    iNum--;
    uint32_t group = iNum / f->superBlock->inodesPerGroup;
    uint32_t indexInGroup = iNum % f->superBlock->inodesPerGroup;

    uint32_t blockSize = 1024 << f->superBlock->logBlockSize;
    uint8_t *bitmap = new uint8_t[blockSize]; // must be blockSize bytes.
    if (f->FetchBlock(groupDesc[group].inodeBitmap, bitmap) != 0)
    {
        std::cerr << "Error fetching inode bitmap for group " << group << std::endl;
        return -1;
    }

    return (bitmap[indexInGroup / 8] >> (indexInGroup % 8)) & 1;
}

uint32_t Inodes::AllocateInode(Ext2File* f, int32_t group)
{
    int32_t start = (group == -1) ? 0 : group;
    int32_t end = (group == -1) ? f->superBlock->inodesCount : group + 1;

    uint32_t blockSize = 1024 << f->superBlock->logBlockSize;
    for (int32_t g = start; g < end; ++g)
    {
        uint8_t *bitmap = new uint8_t[blockSize]; // blockSize bytes for the bitmap.
        if (f->FetchBlock(groupDesc[g].inodeBitmap, bitmap) != 0)
        {
            std::cerr << "Error fetching inode bitmap for group " << g << std::endl;
            continue;
        }

        for (uint32_t i = 0; i < f->superBlock->inodesPerGroup; ++i)
        {
            uint32_t byte = i / 8;
            uint8_t mask = 1 << (i % 8);
            if ((bitmap[byte] & mask) == 0)
            {
                bitmap[byte] |= mask;
                if (f->WriteBlock(groupDesc[g].inodeBitmap, bitmap) != 0)
                {
                    std::cerr << "Error writing inode bitmap for group " << g << std::endl;
                    return 0;
                }
                return g * f->superBlock->inodesPerGroup + i + 1;
            }
        }
    }

    return 0;
}

int32_t Inodes::FreeInode(Ext2File* f, uint32_t iNum)
{
    if (iNum == 0 || iNum > f->superBlock->inodesCount)
        return -1;

    iNum--;
    uint32_t group = iNum / f->superBlock->inodesPerGroup;
    uint32_t indexInGroup = iNum % f->superBlock->inodesPerGroup;

    uint32_t blockSize = 1024 << f->superBlock->logBlockSize;
    uint8_t *bitmap = new uint8_t[blockSize]; // Allocate a buffer for the bitmap.
    if (f->FetchBlock(groupDesc[group].inodeBitmap, bitmap) != 0)
    {
        std::cerr << "Error fetching inode bitmap for group " << group << std::endl;
        return -1;
    }

    bitmap[indexInGroup / 8] &= ~(1 << (indexInGroup % 8));

    if (f->WriteBlock(groupDesc[group].inodeBitmap, bitmap) != 0)
    {
        std::cerr << "Error writing inode bitmap for group " << group << std::endl;
        return -1;
    }

    return 0;
}
