#include <iostream>
#include "MBRPartition.h"

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

void decodeCHS(const uint8_t chs[3], int &cylinder, int &head, int &sector)
{
    head = chs[0];
    sector = chs[1] & 0x3F;
    cylinder = ((chs[1] & 0xC0) << 2) | chs[2];
}

const char* getStatus(uint8_t status)
{
    return (status == 0x80) ? "Active" : "Inactive";
}

const char* getPartitionType(uint8_t partitionType)
{
    if (partitionType == 0x83)
        return "83 linux native";
    else if (partitionType == 0x00)
        return "00 empty";
    else
    {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%02x", partitionType);
        return buf;
    }
}

void displayPartitionEntry(const PartitionEntry &entry, int index)
{
    int cylFirst, headFirst, secFirst;
    int cylLast, headLast, secLast;

    decodeCHS(entry.firstCHS, cylFirst, headFirst, secFirst);
    decodeCHS(entry.lastCHS, cylLast, headLast, secLast);

    std::cout << "Partition table entry " << index << ":\n";
    std::cout << "Status: " << getStatus(entry.status) << "\n";
    std::cout << "First sector CHS: " << cylFirst << "-" << headFirst << "-" << secFirst << "\n";
    std::cout << "Last sector CHS: " << cylLast << "-" << headLast << "-" << secLast << "\n";
    std::cout << "Partition type: " << getPartitionType(entry.partitionType) << "\n";
    std::cout << "First LBA sector: " << entry.firstSector << "\n";
    std::cout << "LBA sector count: " << entry.totalSectors << "\n\n";
}

int main()
{
    char filename[] = "c:/dev/cpp/OS-project/vdi-files/good-dynamic-1k.vdi";
    int partitionIndex = 0;

    MBRPartition mbrPart;
    if (!mbrPart.Open(filename, partitionIndex))
    {
        std::cerr << "Error: Could not open VDI file " << filename << std::endl;
        return 1;
    }

    std::cout << "Output from " << filename << ":\n\n";

    for (int i = 0; i < 4; i++)
    {
        displayPartitionEntry(mbrPart.partitionTable[i], i);
    }

    mbrPart.Close();
    return 0;
}
