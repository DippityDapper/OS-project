#include <cstring>
#include "MBRPartition.h"

bool MBRPartition::Open(char *fn, int part)
{
    vdi = new VDIFile;
    if (!vdi->Open(fn))
    {
        delete vdi;
        vdi = nullptr;
        return false;
    }

    uint8_t mbr[512];
    if (vdi->Read(mbr, 512) != 512)
    {
        vdi->Close();
        delete vdi;
        vdi = nullptr;
        return false;
    }

    for (int i = 0; i < 4; i++)
    {
        memcpy(&partitionTable[i], mbr + 446 + i * 16, sizeof(PartitionEntry));
    }

    if (part < 0 || part > 3)
    {
        Close();
        return false;
    }

    PartitionEntry selected = partitionTable[part];

    partitionOffset = selected.firstSector * 512;
    partitionSize = selected.totalSectors * 512;

    cursor = 0;

    return true;
}

void MBRPartition::Close()
{
    vdi->Close();
    delete vdi;
    vdi = nullptr;
}

ssize_t MBRPartition::Read(void *buf, ssize_t count)
{
    if (cursor >= partitionSize)
        return 0;

    if (cursor + count > partitionSize)
        count = partitionSize - cursor;

    off_t absoluteOffset = partitionOffset + cursor;
    if (vdi->lSeek(static_cast<uint32_t>(absoluteOffset), SEEK_SET_) != static_cast<uint32_t>(absoluteOffset))
        return -1;

    ssize_t bytesRead = vdi->Read(buf, count);
    if (bytesRead < 0)
        return -1;

    cursor += bytesRead;
    return bytesRead;
}

ssize_t MBRPartition::Write(void *buf, size_t count)
{
    if (cursor >= partitionSize)
        return 0;

    if (cursor + count > partitionSize)
        count = partitionSize - cursor;

    off_t absoluteOffset = partitionOffset + cursor;
    if (vdi->lSeek(static_cast<uint32_t>(absoluteOffset), SEEK_SET_) != static_cast<uint32_t>(absoluteOffset))
        return -1;

    ssize_t bytesWritten = vdi->Write(buf, count);
    if (bytesWritten < 0)
        return -1;

    cursor += bytesWritten;
    return bytesWritten;
}

ssize_t MBRPartition::lSeek(ssize_t offset, int whence)
{
    ssize_t newCursor = 0;

    if (whence == SEEK_SET_)
        newCursor = offset;
    else if (whence == SEEK_CUR_)
        newCursor = offset + cursor;
    else if (whence == SEEK_END_)
        newCursor = partitionSize + offset;
    else
        return cursor;

    if (newCursor > partitionSize)
        return cursor;

    cursor = newCursor;
    return cursor;
}
