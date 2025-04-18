#ifndef OS_PROJECT_INODES_H
#define OS_PROJECT_INODES_H

#include <cstdint>

#include "../step-3/Ext2File.h"

#pragma pack(push,1)
struct Inode
{
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t linksCount;
    uint32_t blocks;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15]; // 0-11 direct, 12-14 indirect
    uint32_t generation;
    uint32_t fileAcl;
    uint32_t dirAcl;
    uint32_t faddr;
    uint8_t  osd2[12];
};
#pragma pack(pop)

class Inodes
{
public:
    BlockGroupDescriptor *groupDesc;

    bool SetGroupDesc(Ext2File *f);

    Inodes(Ext2File *f);

    bool FetchInode(Ext2File *f, uint32_t iNum, Inode *buf);
    bool WriteInode(Ext2File* f, uint32_t iNum, Inode* buf);

    bool InodeInUse(Ext2File* f, uint32_t iNum);
    int32_t AllocateInode(Ext2File* f, int32_t group);
    bool FreeInode(Ext2File* f, uint32_t iNum);
};

#endif
