#ifndef OS_VDIFILE_H
#define OS_VDIFILE_H

#include <cstdint>

struct VDIHeader
{
    char     signature[64];
    uint32_t imageSignature;
    uint16_t version1;
    uint16_t version2;
    uint32_t headerSize;
    uint32_t imageType;
    uint32_t flags;
    char     imageDescription[256];
    uint32_t offsetBlocks;
    uint32_t offsetData;
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors;
    uint32_t sectorSize;
    uint32_t unused1;
    uint64_t diskSize;
    uint32_t blockSize;
    uint32_t blockExtraData;
    uint32_t blocksInHDD;
    uint32_t blocksAllocated;
    uint8_t  uuidImage[16];
    uint8_t  uuidLastSnap[16];
    uint8_t  uuidLink[16];
    uint8_t  uuidParent[16];

    uint64_t unused2;         // Unused / Garbage data
    uint64_t unused3;         // Unused / Garbage data
    uint64_t unused4[4];      // More unused data
};

enum
{
    SEEK_SET_,
    SEEK_CUR_,
    SEEK_END_
};

class VDIFile
{
private:
    int fileDescriptor;
    unsigned long long int cursor;
public:
    uint32_t *translationMap;
    VDIHeader *header;

    bool Open(char *fn);
    void Close();
    ssize_t Read(void *buf, size_t count);
    ssize_t Write(void *buf, size_t count);
    uint32_t lSeek(uint32_t offset, int anchor);
};

#endif
