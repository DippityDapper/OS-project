 #include <cstdint>
#include "Inodes.h"
#include <iostream>

 Inodes::Inodes(Ext2File *f)
 {
     SetGroupDesc(f);
 }

 bool Inodes::SetGroupDesc(Ext2File *f)
{
    uint32_t totalBG = (f->superblock->blocksCount + f->superblock->blocksPerGroup - 1) / f->superblock->blocksPerGroup;

    groupDesc = new BlockGroupDescriptor[totalBG];

    uint32_t bgdtBlock = f->superblock->firstDataBlock + 1;
    if (!f->FetchBGDT(bgdtBlock, groupDesc))
    {
        std::cerr << "Failed to fetch BGDT\n";
        delete[] groupDesc;
        return false;
    }
    return true;
}

bool Inodes::FetchInode(Ext2File *f, uint32_t iNum, Inode *buf)
{
    if (iNum == 0 || iNum > f->superblock->inodesCount || !groupDesc)
        return false;

    uint32_t index = iNum - 1;
    uint32_t group = index / f->superblock->inodesPerGroup;
    uint32_t localIndex = index % f->superblock->inodesPerGroup;

    uint32_t blockSize = 1024 << f->superblock->logBlockSize;
    uint32_t inodeSize = f->superblock->inodeSize;
    uint32_t inodesPerBlock   = blockSize / inodeSize;

    uint32_t tableStart = groupDesc[group].inodeTable;
    uint32_t blockOffset = localIndex / inodesPerBlock;
    uint32_t targetBlock = tableStart + blockOffset;

    uint8_t *tmp = new uint8_t[blockSize];
    if (!f->FetchBlock(targetBlock, tmp))
    {
        delete[] tmp;
        return false;
    }

    uint32_t offset = (localIndex % inodesPerBlock) * inodeSize;
    memcpy(buf, tmp + offset, sizeof(Inode));
    delete[] tmp;
    return true;
}

bool Inodes::WriteInode(Ext2File* f, uint32_t iNum, Inode* buf)
{
    if (iNum == 0 || iNum > f->superblock->inodesCount || !groupDesc)
        return false;

    uint32_t index = iNum - 1;
    uint32_t group = index / f->superblock->inodesPerGroup;
    uint32_t localIndex = index % f->superblock->inodesPerGroup;

    uint32_t blockSize = 1024 << f->superblock->logBlockSize;
    uint32_t inodeSize = f->superblock->inodeSize;
    uint32_t inodesPerBlock   = blockSize / inodeSize;

    uint32_t tableStart = groupDesc[group].inodeTable;
    uint32_t blockOffset = localIndex / inodesPerBlock;
    uint32_t targetBlock = tableStart + blockOffset;

    uint8_t *tmp = new uint8_t[blockSize];
    if (!f->FetchBlock(targetBlock, tmp))
    {
        delete[] tmp;
        return false;
    }

    uint32_t offset = (localIndex % inodesPerBlock) * inodeSize;
    memcpy(tmp + offset, buf, sizeof(Inode));

    if (!f->WriteBlock(targetBlock, tmp))
    {
        delete[] tmp;
        return false;
    }

    delete[] tmp;
    return true;
}

 bool Inodes::InodeInUse(Ext2File* f, uint32_t iNum)
 {
     if (iNum == 0 || iNum > f->superblock->inodesCount)
         return false;

     uint32_t index = iNum - 1;

     if (f->superblock->inodesPerGroup == 0)
         return false;
     uint32_t group = index / f->superblock->inodesPerGroup;
     uint32_t localIndex = index % f->superblock->inodesPerGroup;

     uint32_t blockSize = 1024 << f->superblock->logBlockSize;
     uint32_t bitmapBlockNum = groupDesc[group].inodeBitmap;

     uint8_t *buf= new uint8_t[blockSize];
     if (!f->FetchBlock(bitmapBlockNum, buf))
         return false;

     uint32_t byteOff = localIndex / 8;
     uint32_t bitOff  = localIndex % 8;

     uint32_t value = (buf[byteOff] & (1u << bitOff));
     delete[] buf;
     return value != 0;
 }

 int32_t Inodes::AllocateInode(Ext2File* f, int32_t group)
 {
     uint32_t inodesPerGroup = f->superblock->inodesPerGroup;
     uint32_t blockSize = 1024 << f->superblock->logBlockSize;

     uint8_t *buf= new uint8_t[blockSize];
     if (!f->FetchBlock(groupDesc[group].inodeBitmap, buf))
     {
         delete[] buf;
         return -1;
     }

     bool allocated = false;
     uint32_t newIno = 0;
     uint32_t bytes = (inodesPerGroup + 7) / 8;

     for (uint32_t b = 0; b < bytes; ++b)
     {
         if (allocated)
             break;

         if (buf[b] != 0xFF)
         {
             for (int bit = 0; bit < 8; ++bit)
             {
                 uint32_t idx = b * 8 + bit;

                 if (idx >= inodesPerGroup)
                     break;

                 if (!(buf[b] & (1u << bit)))
                 {
                     buf[b] |= (1u << bit);
                     newIno = group * inodesPerGroup + idx + 1;
                     allocated = true;
                     break;
                 }
             }
         }
     }

     if (!allocated)
     {
         delete[] buf;
         return -1;
     }

     if (!f->WriteBlock(groupDesc[group].inodeBitmap, buf))
     {
         delete[] buf;
         return -1;
     }

     f->superblock->freeInodesCount--;
     groupDesc[group].freeInodesCount--;
     f->WriteSuperBlock(f->superblock->firstDataBlock, f->superblock);

     uint32_t descsPerBlock = blockSize / sizeof(BlockGroupDescriptor);
     uint32_t bgdtBlock = f->superblock->firstDataBlock + 1 + (group / descsPerBlock);

     f->WriteBGDT(bgdtBlock,&groupDesc[(group / descsPerBlock) * descsPerBlock]);

     delete[] buf;
     return static_cast<int32_t>(newIno);
 }

 bool Inodes::FreeInode(Ext2File* f, uint32_t iNum)
 {
     if (iNum == 0 || iNum > f->superblock->inodesCount)
         return false;

     uint32_t index = iNum - 1;
     uint32_t group = index / f->superblock->inodesPerGroup;
     uint32_t localIndex = index % f->superblock->inodesPerGroup;

     uint32_t blockSize = 1024 << f->superblock->logBlockSize;
     uint32_t bitmapBlockNum = groupDesc[group].inodeBitmap;

     uint8_t *buf= new uint8_t[blockSize];
     if (!f->FetchBlock(bitmapBlockNum, buf))
     {
         delete[] buf;
         return false;
     }

     uint32_t byteOff = localIndex / 8;
     uint32_t bitOff = localIndex % 8;

     buf[byteOff] &= ~(1u << bitOff);

     if (!f->WriteBlock(bitmapBlockNum, buf))
     {
         delete[] buf;
         return false;
     }

     f->superblock->freeInodesCount++;
     groupDesc[group].freeInodesCount++;
     f->WriteSuperBlock(f->superblock->firstDataBlock, f->superblock);
     f->WriteBGDT(f->superblock->firstDataBlock + 1, groupDesc);

     delete[] buf;
     return true;
 }
