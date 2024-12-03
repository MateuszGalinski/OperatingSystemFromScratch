#include "stdint.h"
#include "stdio.h"
#include "disk.h"
#include "fat.h"

void far* g_data = (void far*)0x00500200;

void _cdecl cstart_(uint16_t bootDrive){
    puts("Hello world from C!\r\n");

    printf("Formatted %% %c %s\r\n", 'a', "string");
    printf("Formatted %d %d\r\n", 1234, -5678);
    printf("Formatted %u\r\n", 121);
    printf("\r\n");

    DISK disk;
    if (!DISK_Initialize(&disk, bootDrive))
    {
        printf("Disk init error\r\n");
        goto end;
    }

    DISK_ReadSectors(&disk, 19, 1, g_data);

    if (!FAT_Initialize(&disk))
    {
        printf("FAT init error\r\n");
        goto end;
    }

    // browse files in root
    FAT_File far* fd = FAT_Open(&disk, "/");
    FAT_DirectoryEntry entry;
    int i = 0;
    while (FAT_ReadEntry(&disk, fd, &entry) && i++ < 5)
    {
        for (int j = 0; j < 11; j++){
            putc(entry.name[j]);
        }
        printf(" | SIZE of file %d", entry.size);
        printf("\r\n");
    }
    FAT_Close(fd);

    printf("\r\n------------------------\r\n");
    printf("CONTENT OF TESTDIR:\r\n");
    printf("\r\n");

    //browse files in testdir
    FAT_File far* testDir = FAT_Open(&disk, "/testdir/");
    i = 0;
    while (FAT_ReadEntry(&disk, testDir, &entry) && i++ < 5)
    {
        for (int j = 0; j < 11; j++) {
            putc(entry.name[j]);
        }
        printf("\r\n");
    }
    FAT_Close(testDir);


    printf("\r\n------------------------\r\n");
    printf("CONTENT OF .TXT FILE:\r\n");
    printf("\r\n");
    // read test.txt
    char buffer[100];
    uint32_t read;
    fd = FAT_Open(&disk, "test.txt");
    while ((read = FAT_Read(&disk, fd, sizeof(buffer), buffer)))
    {
        for (uint32_t j = 0; j < read; j++)
        {
            if (buffer[j] == '\n'){
                putc('\r');
            }

            putc(buffer[j]);
        }
    }
    FAT_Close(fd);

end:
    for (;;);
}
