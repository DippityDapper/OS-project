#include <iostream>
#include <cstdint>
#include <fcntl.h>
#include "../step-3/Ext2File.h"
#include "../step-4/Inodes.h"
#include <sys/stat.h>   // for S_IFREG

void DisplayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset)
{
    if (count > 256)
        count = 256;

    printf("Offset: 0x%02llx\n", offset);

    for (uint32_t i = 0; i < 256; i += 16)
    {
        if (i >= skip + count)
            break;

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

std::string FormatTimestamp(uint32_t epoch)
{
    time_t rawtime = static_cast<time_t>(epoch);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", localtime(&rawtime));
    return (buffer);
}

std::string FormatMode(uint16_t mode)
{
    std::string out;

    // File type
    if ((mode & 0xF000) == 0x4000) out += 'd';
    else if ((mode & 0xF000) == 0x8000) out += '-';
    else if ((mode & 0xF000) == 0xA000) out += 'l';
    else out += '?';

    // User, group, other
    const char* perms = "rwx";
    for (int i = 0; i < 9; ++i)
        out += (mode & (1 << (8 - i))) ? perms[i % 3] : '-';

    return out;
}


void DisplayInode(uint32_t inodeNum, Inode *inode)
{
    printf("Inode: %u\n", inodeNum );
    printf("Mode: %u - %s\n", inode->mode, FormatMode(inode->mode).c_str());
    printf("Size: %u\n", inode->size);
    printf("Blocks: %u\n", inode->blocks);
    printf("UID/GID: %u / %u\n", inode->uid, inode->gid);
    printf("Links: %u\n", inode->linksCount);
    printf("Created: %s\n", FormatTimestamp(inode->ctime).c_str());
    printf("Last access: %s\n", FormatTimestamp(inode->atime).c_str());
    printf("Last modification: %s\n", FormatTimestamp(inode->mtime).c_str());
    printf("Deleted: %s\n", FormatTimestamp(inode->dtime).c_str());
    printf("Flags: %u\n", inode->flags);
    printf("File version: %u\n", inode->generation);
    printf("ACL block: %u\n", inode->fileAcl);

    printf("Direct blocks:\n0-3: ");
    for (int i = 0; i < 4; ++i) printf(" %u", inode->block[i] );
    printf("\n4-7: ");
    for (int i = 4; i < 8; ++i) printf(" %u", inode->block[i] );
    printf("\n8-11: ");
    for (int i = 8; i < 12; ++i) printf(" %u", inode->block[i] );
    printf("\n");

    printf("Single indirect block: %u\n", inode->block[12]);
    printf("Double indirect block: %u\n", inode->block[13]);
    printf("Triple indirect block: %u\n", inode->block[14]);
}

bool ResolveBlockPointerRaw(Ext2File *f, Inodes *inodes, uint32_t iNum, Inode *inode, uint32_t bNum, uint8_t *scratch, bool allocate, uint32_t &outBlock)
{
    uint32_t blockSize = 1024u << f->superblock->logBlockSize;
    uint32_t k = blockSize / sizeof(uint32_t);

    int depth;
    uint32_t index0 = 0;
    uint32_t index1 = 0;
    uint32_t index2 = 0;

    if (bNum < 12)
    {
        depth = 0;
        index0 = bNum;
    }
    else if (bNum < 12 + k)
    {
        depth = 1;
        index0 = bNum - 12;
        index1 = index0;
    }
    else if (bNum < 12 + k + k*k)
    {
        depth = 2;
        uint32_t rem = bNum - 12;
        index1 = rem / k;
        index0 = rem % k;
    }
    else
    {
        depth = 3;
        uint32_t rem = bNum - 12 - k;
        index2 = rem / (k * k);
        rem  = rem % (k*k);
        index1 = rem / k;
        index0 = rem % k;
    }

    uint32_t *ptrs[4] = { nullptr, nullptr, nullptr, nullptr };

    if (depth == 0)
        ptrs[0] = &inode->block[index0];
    else
        ptrs[0] = &inode->block[12];

    if (depth >= 1)
        ptrs[1] = &inode->block[12];
    if (depth >= 2)
        ptrs[2] = &inode->block[13];
    if (depth >= 3)
        ptrs[3] = &inode->block[14];

    uint32_t indices[4] = {index0, index1, index2, 0 };

    for (int lvl = 0; lvl <= depth; lvl++)
    {
        if (*ptrs[lvl] == 0)
        {
            if (!allocate)
                return false;

            *ptrs[lvl] = f->AllocateBlock();
            if (*ptrs[lvl] == 0)
                return false;

            if (lvl > 0)
            {
                memset(scratch, 0, blockSize);
                f->WriteBlock(*ptrs[lvl], scratch);
            }
        }

        if (lvl == depth) {
            outBlock = *ptrs[lvl];
            break;
        }

        if (!f->FetchBlock(*ptrs[lvl], scratch))
            return false;

        uint32_t* array = reinterpret_cast<uint32_t*>(scratch);
        ptrs[lvl+1] = &array[ indices[lvl+1] ];
    }

    if (allocate)
        inodes->WriteInode(f, iNum, inode);

    return true;
}

bool FetchBlockFromFile(Ext2File *f, Inode *i, uint32_t bNum, void *buf)
{
    uint32_t blockSize = 1024u << f->superblock->logBlockSize;
    uint8_t* scratch = new uint8_t[blockSize];
    uint32_t physBlock = 0;

    bool result = ResolveBlockPointerRaw(f, nullptr, 0, i, bNum, scratch, false, physBlock);

    if (result)
        result = f->FetchBlock(physBlock, buf);

    delete[] scratch;
    return result;
}

bool WriteBlockToFile(Ext2File *f, Inodes *inodes, uint32_t iNum, Inode *i, uint32_t bNum, void *buf)
{
    uint32_t blockSize = 1024u << f->superblock->logBlockSize;
    uint8_t *scratch = new uint8_t[blockSize];
    uint32_t physBlock = 0;

    bool result = ResolveBlockPointerRaw(f, inodes, iNum, i, bNum, scratch, true, physBlock);

    if (result)
        result = f->WriteBlock(physBlock, buf);

    delete[] scratch;
    return result;
}

int main()
{
    char filename[] = "c:/dev/cpp/OS-project/vdi-files/good-dynamic-1k.vdi";
    Ext2File *extFile = new Ext2File;
    if (!extFile->Open(filename))
        return -1;

    uint32_t inodeNum = 2; // root dir
    Inodes *inodes = new Inodes(extFile);

    Inode *inode = new Inode;
    if (!inodes->FetchInode(extFile, inodeNum, inode))
    {
        std::cerr << "Failed to fetch inode " << inodeNum << "\n";
        return -1;
    }
    DisplayInode(inodeNum, inode);

    uint32_t blockSize = 1024 << extFile->superblock->logBlockSize;
    uint8_t *buf = new uint8_t[blockSize];

    if (FetchBlockFromFile(extFile, inode, 0, buf))
    {
        std::cout << "Block contents:" << std::endl;
        DisplayBuffer(buf, blockSize, 0);
    }
    else
    {
        std::cout << "Failed to fetch block or block not allocated" << std::endl;
    }

    std::cout << "\n\n\n";

    inodeNum = 11;
    if (!inodes->FetchInode(extFile, inodeNum, inode))
    {
        std::cerr << "Failed to fetch inode " << inodeNum << "\n";
        return -1;
    }

    DisplayInode(inodeNum, inode);

    if (FetchBlockFromFile(extFile, inode, 0, buf))
    {
        std::cout << "Block contents:" << std::endl;
        DisplayBuffer(buf, blockSize, 0);
    }
    else
    {
        std::cout << "Failed to fetch block or block not allocated" << std::endl;
    }

    extFile->Close();
    delete extFile;
    delete inode;
    delete inodes;
    delete[] buf;
    return 0;
}
