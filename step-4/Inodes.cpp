 #include <cstdint>
#include <cstring>
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
    if (!f->FetchBGDT(bgdtBlock, groupDesc)) {
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
    memcpy(buf, tmp + offset, sizeof(Inode));

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
     // invalid?
     if (iNum == 0 || iNum > f->superblock->inodesCount)
         return false;

     uint32_t index = iNum - 1;
     uint32_t group = index / f->superblock->inodesPerGroup;
     uint32_t localIndex = index % f->superblock->inodesPerGroup;

     uint32_t blockSize = 1024 << f->superblock->logBlockSize;
     uint32_t bitmapBlockNum = groupDesc[group].inodeBitmap;

     // read the bitmap
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
     uint32_t ngroups = (f->superblock->inodesCount + f->superblock->inodesPerGroup - 1) / f->superblock->inodesPerGroup;
     uint32_t inodesPerGroup = f->superblock->inodesPerGroup;
     uint32_t blockSize = 1024 << f->superblock->logBlockSize;

     auto try_group = [&](uint32_t g) -> int32_t
     {
         // load bitmap
         uint8_t *buf= new uint8_t[blockSize];
         if (!f->FetchBlock(groupDesc[g].inodeBitmap, buf))
         {
             delete[] buf;
             return -1;
         }

         uint32_t bytes = (inodesPerGroup + 7) / 8;
         for (uint32_t b = 0; b < bytes; ++b)
         {
             if (buf[b] != 0xFF)
             { // has a zero bit
                 for (int bit = 0; bit < 8; ++bit)
                 {
                     uint32_t idx = b * 8 + bit;
                     if (idx >= inodesPerGroup)
                         break;
                     if ((buf[b] & (1u << bit)) == 0)
                     {
                         // mark it
                         buf[b] |= (1u << bit);
                         if (!f->WriteBlock(groupDesc[g].inodeBitmap, buf))
                         {
                             delete[] buf;
                             return -1;
                         }

                         // compute global inode number (1-based)
                         uint32_t newIno = g * inodesPerGroup + idx + 1;

                         // update counts
                         f->superblock->freeInodesCount--;
                         groupDesc[g].freeInodesCount--;
                         // write superblock and BGDT back
                         f->WriteSuperBlock(f->superblock->firstDataBlock, f->superblock);
                         f->WriteBGDT(f->superblock->firstDataBlock + 1, groupDesc);

                         delete[] buf;
                         return static_cast<int32_t>(newIno);
                     }
                 }
             }
         }
         delete[] buf;
         return -1;
     };

     // choose which group(s) to try
     if (group >= 0 && static_cast<uint32_t>(group) < ngroups)
     {
         return try_group(group);
     }
     else if (group == -1)
     {
         for (uint32_t g = 0; g < ngroups; ++g)
         {
             int32_t ino = try_group(g);
             if (ino > 0)
                 return ino;
         }
         return -1;
     }
     else
     {
         return -1;
     }
 }

 bool Inodes::FreeInode(Ext2File* f, uint32_t iNum)
 {
     // invalid?
     if (iNum == 0 || iNum > f->superblock->inodesCount)
         return false;

     uint32_t index = iNum - 1;
     uint32_t group = index / f->superblock->inodesPerGroup;
     uint32_t localIndex = index % f->superblock->inodesPerGroup;

     uint32_t blockSize = 1024 << f->superblock->logBlockSize;
     uint32_t bitmapBlockNum = groupDesc[group].inodeBitmap;

     // load bitmap
     uint8_t *buf= new uint8_t[blockSize];
     if (!f->FetchBlock(bitmapBlockNum, buf))
     {
         delete[] buf;
         return false;
     }

     uint32_t byteOff = localIndex / 8;
     uint32_t bitOff  = localIndex % 8;

     // clear bit
     buf[byteOff] &= ~(1u << bitOff);

     if (!f->WriteBlock(bitmapBlockNum, buf))
     {
         delete[] buf;
         return false;
     }

     // update counts
     f->superblock->freeInodesCount++;
     groupDesc[group].freeInodesCount++;
     f->WriteSuperBlock(f->superblock->firstDataBlock, f->superblock);
     f->WriteBGDT(f->superblock->firstDataBlock + 1, groupDesc);

     delete[] buf;
     return true;
 }
