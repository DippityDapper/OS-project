#include "../step-4/Inodes.h"
#include <cstdio>
#include <ctime>
#include <cstring>
#include <iostream>

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

int main()
{
    char filename[] = "c:/dev/cpp/OS-project/vdi-files/good-dynamic-2k.vdi";
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
    extFile->Close();
    delete extFile;
    delete inode;
    delete inodes;
    return 0;
}
