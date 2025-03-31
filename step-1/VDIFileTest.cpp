#include <iostream>
#include <cstring>
#include <iomanip>
#include "VDIFile.h"

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

VDIFile *TestVDIOpen(const char *filePath)
{
    VDIFile *vdi = new VDIFile;
    if (vdi->Open(const_cast<char *>(filePath)))
        std::cout << "Open: Success" << std::endl;
    else
        std::cerr << "Open: Failed" << std::endl;
    return vdi;
}

void TestVDIRead(VDIFile *vdi, size_t count)
{
    char *buffer[count];
    ssize_t bytesRead = vdi->Read(buffer, sizeof(buffer));

    DisplayBuffer(reinterpret_cast<uint8_t *>(buffer), sizeof(buffer), 0);

    if (bytesRead > 0)
        std::cout << "Read: Read " << bytesRead << " bytes" << std::endl;
    else
        std::cerr << "Read: Failed" << std::endl;
}

void TestVDISeek(VDIFile *vdi, uint32_t offset, int anchor)
{
    uint32_t newPos = vdi->lSeek(offset, anchor);
    std::cout << "lSeek: New position = " << newPos << std::endl;
}

void TestVDIWrite(VDIFile *vdi)
{
    uint8_t *buffer[4];
    memset(buffer, 0xAB, sizeof(buffer));
    ssize_t bytesWritten = vdi->Write(buffer, sizeof(buffer));

    if (bytesWritten > 0)
        std::cout << "Write: Wrote " << bytesWritten << " bytes" << std::endl;
    else
        std::cerr << "Write: Failed" << std::endl;
}

void PrintUUID(uint8_t uuid[16])
{
    for (int i = 0; i < 16; i++)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            std::cout << "-";
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)uuid[i];
    }
    std::cout << std::dec;
}

void TestVDIHeader(VDIFile *vdi)
{
    if (!vdi || !vdi->header)
    {
        std::cerr << "VDIHeader is not available!" << std::endl;
        return;
    }

    std::cout << "\nDump of VDI header:" << std::endl;
    std::cout << "  Image name: [" << std::string(vdi->header->signature, 64) << "]\n";
    std::cout << "  Signature: 0x" << std::hex << vdi->header->imageSignature << "\n";
    std::cout << "  Version: " << std::dec << vdi->header->version1 << "." << vdi->header->version2 << "\n";
    std::cout << "  Header size: 0x" << std::hex << vdi->header->headerSize << " " << std::dec << vdi->header->headerSize << "\n";
    std::cout << "  Image type: 0x" << std::hex << vdi->header->imageType << "\n";
    std::cout << "  Flags: 0x" << std::hex << vdi->header->flags << "\n";
    std::cout << "  Virtual CHS: " << vdi->header->cylinders << "-" << vdi->header->heads << "-" << vdi->header->sectors << "\n";
    std::cout << "  Sector size: " << vdi->header->sectorSize << " " << std::dec << vdi->header->sectorSize << "\n";
    std::cout << "  Map offset: 0x" << std::hex << vdi->header->offsetBlocks << " " << std::dec << vdi->header->offsetBlocks << "\n";
    std::cout << "  Frame offset: 0x" << std::hex << vdi->header->offsetData << " " << std::dec << vdi->header->offsetData << "\n";
    std::cout << "  Frame size: 0x" << std::hex << vdi->header->blockSize << " " << std::dec << vdi->header->blockSize << "\n";
    std::cout << "  Extra frame size: 0x" << std::hex << vdi->header->blockExtraData << " " << std::dec << vdi->header->blockExtraData << "\n";
    std::cout << "  Total frames: 0x" << std::hex << vdi->header->blocksInHDD << " " << std::dec << vdi->header->blocksInHDD << "\n";
    std::cout << "  Frames allocated: 0x" << std::hex << vdi->header->blocksAllocated << " " << std::dec << vdi->header->blocksAllocated << "\n";
    std::cout << "  Disk size: 0x" << std::hex << vdi->header->diskSize << " " << std::dec << vdi->header->diskSize << "\n";
    std::cout << "  UUID: "; PrintUUID(vdi->header->uuidImage); std::cout << "\n";
    std::cout << "  Last snap UUID: "; PrintUUID(vdi->header->uuidLastSnap); std::cout << "\n";
    std::cout << "  Link UUID: "; PrintUUID(vdi->header->uuidLink); std::cout << "\n";
    std::cout << "  Parent UUID: "; PrintUUID(vdi->header->uuidParent); std::cout << "\n";
}

int main()
{
    VDIFile *vdi = TestVDIOpen("c:/dev/cpp/OS-project/vdi-files/good-dynamic-1k.vdi");
    if (vdi)
    {
        TestVDIHeader(vdi);

//        TestVDIWrite(vdi);
//        TestVDISeek(vdi, 16, SEEK_SET_);
//        TestVDISeek(vdi, 16, SEEK_CUR_);
//        TestVDIRead(vdi, 32);
        DisplayBuffer(reinterpret_cast<uint8_t *>(vdi->translationMap), vdi->header->blocksInHDD * 4, 0);

        vdi->Close();
    }
    return 0;
}
