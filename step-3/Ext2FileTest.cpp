#include "../step-3/Ext2File.h"
#include <cstdio>
#include <ctime>
#include <cstring>
#include <cctype>
#include <iostream>

void DisplayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset)
{
    if (count > 256)
        count = 256;

    printf("Offset: 0x%02llx\n", offset);

    for (uint32_t i = 0; i < 256; i += 16)
    {
        if (i >= skip + count) break;

        printf("%02x: ", i);

        for (uint32_t j = 0; j < 16; j++)
        {
            uint32_t index = i + j;
            if (index >= skip && index < skip + count && index < 256)
                printf("%02x ", buf[index]);
            else if (index < 256)
                printf("   ");
        }

        printf(" | ");

        for (uint32_t j = 0; j < 16; j++)
        {
            uint32_t index = i + j;
            if (index >= skip && index < skip + count && index < 256)
                printf("%c", isprint(buf[index]) ? buf[index] : ' ');
            else if (index < 256)
                printf(" ");
        }

        printf("\n");
    }

    printf("\n");
}

void DisplayBuffer(uint8_t *buf, uint32_t count, uint64_t offset)
{
    uint32_t remainingBytes = count;
    uint32_t pageSize = 256;

    while (remainingBytes > 0)
    {
        uint32_t bytesToDisplay = remainingBytes > pageSize ? pageSize : remainingBytes;

        DisplayBufferPage(buf, bytesToDisplay, 0, offset);

        buf += bytesToDisplay;
        offset += bytesToDisplay;
        remainingBytes -= bytesToDisplay;
    }
}

void FormatUUID(const uint8_t uuid[16], char outStr[37])
{
    sprintf(outStr,
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            uuid[0], uuid[1], uuid[2], uuid[3],
            uuid[4], uuid[5],
            uuid[6], uuid[7],
            uuid[8], uuid[9],
            uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
}

void PrintTime(uint32_t epochTime)
{
    time_t t = epochTime;
    char buf[26];
    ctime_s(buf, 26, &t);
    buf[strcspn(buf, "\n")] = '\0'; // Remove newline
    printf("%s", buf);
}

void PrintSuperBlock(const SuperBlock *sb)
{
    uint32_t blockSize = 1024 << sb->logBlockSize;
    uint32_t fragSize = (sb->logFragSize < 0) ? (1024 >> (-sb->logFragSize)) : (1024 << sb->logFragSize);

    char uuidStr[37];
    char journalUuidStr[37];
    FormatUUID(sb->uuid, uuidStr);
    FormatUUID(sb->journalUuid, journalUuidStr);

    const char *revisionStr = (sb->revLevel == 1) ? "1.0" : "0.0";

    printf("Superblock contents:\n");
    printf("Number of inodes: %u\n", sb->inodesCount);
    printf("Number of blocks: %u\n", sb->blocksCount);
    printf("Number of reserved blocks: %u\n", sb->rBlocksCount);
    printf("Number of free blocks: %u\n", sb->freeBlocksCount);
    printf("Number of free inodes: %u\n", sb->freeInodesCount);
    printf("First data block: %u\n", sb->firstDataBlock);
    printf("Log block size: %u (%u)\n", sb->logBlockSize, blockSize);
    printf("Log fragment size: %d (%u)\n", sb->logFragSize, fragSize);
    printf("Blocks per group: %u\n", sb->blocksPerGroup);
    printf("Fragments per group: %u\n", sb->fragsPerGroup);
    printf("Inodes per group: %u\n", sb->inodesPerGroup);

    printf("Last mount time: ");
    PrintTime(sb->mtime);
    printf("\n");

    printf("Last write time: ");
    PrintTime(sb->wtime);
    printf("\n");

    printf("Mount count: %u\n", sb->mntCount);
    printf("Max mount count: %d\n", sb->maxMntCount);
    printf("Magic number: 0x%04x\n", sb->magic);
    printf("State: %u\n", sb->state);
    printf("Error processing: %u\n", sb->errors);
    printf("Revision level: %s\n", revisionStr);
    printf("Last system check: ");
    PrintTime(sb->lastcheck);
    printf("\n");
    printf("Check interval: %u\n", sb->checkinterval);
    printf("OS creator: %u\n", sb->creatorOS);
    printf("Default reserve UID: %u\n", sb->defResuid);
    printf("Default reserve GID: %u\n", sb->defResgid);
    printf("First inode number: %u\n", sb->firstIno);
    printf("Inode size: %u\n", sb->inodeSize);
    printf("Block group number: %u\n", sb->blockGroupNr);
    printf("Feature compatibility bits: 0x%08x\n", sb->featureCompat);
    printf("Feature incompatibility bits: 0x%08x\n", sb->featureIncompat);
    printf("Feature read/only compatibility bits: 0x%08x\n", sb->featureROCompat);
    printf("UUID: %s\n", uuidStr);
    printf("Volume name: [%.*s]\n", 16, sb->volumeName);
    printf("Last mount point: [%.*s]\n", 64, sb->lastMounted);
    printf("Algorithm bitmap: 0x%08x\n", sb->algoBitmap);
    printf("Number of blocks to preallocate: %u\n", sb->preallocBlocks);
    printf("Number of blocks to preallocate for directories: %u\n", sb->preallocDirBlocks);
    printf("Journal UUID: %s\n", journalUuidStr);
    printf("Journal inode number: %u\n", sb->journalInum);
    printf("Journal device number: %u\n", sb->journalDev);
    printf("Journal last orphan inode number: %u\n", sb->lastOrphan);
    printf("Default hash version: %u\n", sb->defHashVersion);
    printf("Default mount option bitmap: 0x%08x\n", sb->defaultMountOptions);
    printf("First meta block group: %u\n", sb->firstMetaBg);
}

void PrintBlockGroupDescriptorTable(uint32_t group, Ext2File *extFile)
{
    printf("Block group descriptor table:\n");
    printf("Group\tBlock Bitmap\tInode Bitmap\tInode Table\tFree Blocks\tFree Inodes\tUsed Dirs\n");
    printf("-----\t------------\t------------\t-----------\t-----------\t-----------\t--------\n");

    uint32_t numBlockGroups = (extFile->superBlock->blocksCount + extFile->superBlock->blocksPerGroup - 1) / extFile->superBlock->blocksPerGroup;

    uint32_t bgdtStartBlock;
    if (group == 0)
        bgdtStartBlock = 1;
    else
        bgdtStartBlock = group * extFile->superBlock->blocksPerGroup + 1;

    BlockGroupDescriptor *bgdt = new BlockGroupDescriptor[numBlockGroups];

    if (!extFile->FetchBGDT(bgdtStartBlock, bgdt))
    {
        std::cerr << "Failed to fetch the entire BGDT.\n";
        delete[] bgdt;
        return;
    }

    for (uint32_t i = 0; i < numBlockGroups; i++)
    {
        printf("%6u\t%12u\t%12u\t%11u\t%11u\t%11u\t%8u\n",
               i,
               bgdt[i].blockBitmap,
               bgdt[i].inodeBitmap,
               bgdt[i].inodeTable,
               bgdt[i].freeBlocksCount,
               bgdt[i].freeInodesCount,
               bgdt[i].usedDirsCount);
    }

    printf("\n");
    delete[] bgdt;
}

bool PrintGroup(uint32_t group, Ext2File *extFile, SuperBlock *sb)
{
    uint32_t groupStartBlock = group * extFile->superBlock->blocksPerGroup;

    if (!extFile->FetchSuperBlock(groupStartBlock, sb))
    {
        std::cerr << "Failed to fetch superblock from group "<< group << "\n";
        extFile->Close();
        delete extFile;
        return false;
    }
    printf("Superblock from block %u\n", groupStartBlock);
    PrintSuperBlock(sb);
    //PrintBlockGroupDescriptorTable(group, extFile);
    std::cout << "\n\n\n";
    return true;
}

int main()
{
    Ext2File *extFile = new Ext2File;
    char filename[] = "c:/dev/cpp/OS-project/vdi-files/good-dynamic-2k.vdi";
    if (!extFile->Open(filename))
    {
        std::cerr << "Failed to open file: " << filename << "\n";
        return -1;
    }

    SuperBlock *sb = extFile->superBlock;

    uint32_t  totalBG = (sb->blocksCount + sb->blocksPerGroup - 1) / sb->blocksPerGroup;
    for (int i = 0; i < totalBG; i++)
    {
        if (i > 9)
            continue;

        if (!PrintGroup(i, extFile, sb))
            return 1;
        if (i == 0)
            continue;
        i++;
    }

    extFile->Close();
    delete extFile;
    return 0;
}
