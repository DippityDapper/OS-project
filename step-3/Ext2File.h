#ifndef OS_PROJECT_EXT2FILE_H
#define OS_PROJECT_EXT2FILE_H

#include "../step-2/MBRPartition.h"

#ifndef OS_EXT2SUPERBLOCK_H
#define OS_EXT2SUPERBLOCK_H

#include <cstdint>


struct SuperBlock
{
    uint32_t inodesCount;
    uint32_t blocksCount;
    uint32_t rBlocksCount;
    uint32_t freeBlocksCount;
    uint32_t freeInodesCount;
    uint32_t firstDataBlock;
    uint32_t logBlockSize;
    int32_t  logFragSize;
    uint32_t blocksPerGroup;
    uint32_t fragsPerGroup;
    uint32_t inodesPerGroup;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mntCount;
    int16_t  maxMntCount;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minorRevLevel;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creatorOS;
    uint32_t revLevel;
    uint16_t defResuid;
    uint16_t defResgid;
    uint32_t firstIno;
    uint16_t inodeSize;
    uint16_t blockGroupNr;
    uint32_t featureCompat;
    uint32_t featureIncompat;
    uint32_t featureROCompat;
    uint8_t  uuid[16];
    char     volumeName[16];
    char     lastMounted[64];
    uint32_t algoBitmap;
    uint8_t  preallocBlocks;
    uint8_t  preallocDirBlocks;
    uint8_t  journalUuid[16];
    uint32_t journalInum;
    uint32_t journalDev;
    uint32_t lastOrphan;
    uint32_t hashSeed[4];
    uint8_t  defHashVersion;
    uint32_t defaultMountOptions;
    uint32_t firstMetaBg;
};

#pragma pack(push, 1)
struct BlockGroupDescriptor
{
    uint32_t blockBitmap;
    uint32_t inodeBitmap;
    uint32_t inodeTable;
    uint16_t freeBlocksCount;
    uint16_t freeInodesCount;
    uint16_t usedDirsCount;
    uint16_t pad;
    char reserved[12];
};
#pragma pack(pop)
#endif


class Ext2File
{
public:
    MBRPartition *mbrPart;
    SuperBlock *superblock;

    bool Open(char *fn);
    void Close();

    bool FetchBlock(uint32_t blockNum, void *buf);
    bool WriteBlock(uint32_t blockNum, void *buf);

    bool FetchSuperBlock(uint32_t blockNum, struct SuperBlock *sb);
    bool WriteSuperBlock(uint32_t blockNum, struct SuperBlock *sb);

    bool FetchBGDT(uint32_t blockNum, BlockGroupDescriptor *bgdt);
    bool WriteBGDT(uint32_t blockNum, BlockGroupDescriptor *bgdt);
};


#endif
