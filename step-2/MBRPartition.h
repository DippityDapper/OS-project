#ifndef OS_PROJECT_MBRPARTITION_H
#define OS_PROJECT_MBRPARTITION_H

#include <cstdio>
#include "../step-1/VDIFile.h"

struct PartitionEntry
{
    uint8_t status;
    uint8_t firstCHS[3];
    uint8_t partitionType;
    uint8_t lastCHS[3];
    int32_t firstSector;
    uint32_t totalSectors;
};

class MBRPartition
{
private:
    off_t cursor;
public:
    VDIFile *vdi;
    PartitionEntry partitionTable[4];
    uint32_t partitionOffset;
    uint32_t partitionSize;

    bool Open(char *fn, int part);
    void Close();
    ssize_t Read(void *buf, ssize_t count);
    ssize_t Write(void *buf, size_t count);
    ssize_t lSeek(ssize_t offset, int whence);
};

#endif
