#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef uint8_t bool;
#define true 1
#define false 0
#define TWELVE_BITS_CONVERSION 3/2

typedef struct {
    uint8_t BootJumpInstruction[3]; //bdb_oem:                    db 'MSWIN4.1'
    uint8_t oemIdentifier[8];       //bdb_bytes_per_sector:       dw 512
    uint16_t bytesPerSector;        //bdb_sectors_per_cluster:    db 1
    uint8_t sectorsPerCluster;      // bdb_sectors_per_cluster:    db 1
    uint16_t reservedSectors;       // bdb_reserved_sectors:       dw 1
    uint8_t fatCount;               // bdb_fat_count:              db 2
    uint16_t dirEntryCount;         // bdb_dir_entries_count:      dw 0E0h
    uint16_t totalSectors;          // bdb_total_sectors:          dw 2880 ; 2880 * 512 = 1.44MB
    uint8_t mediaDescriptorType;    // bdb_media_descriptor_type:  db 0F0h ; F0 = 3.5" floppy disk
    uint16_t sectorsPerFat;         // bdb_sectors_per_fat:        dw 9
    uint16_t sectorsPerTrack;       // bdb_sector_per_track:       dw 18
    uint16_t heads;                 // bdb_heads:                  dw 2
    uint32_t hiddenSectors;         // bdb_hidden_sectors:         dd 0
    uint32_t largeSectorCount;      // bdb_large_sector_count:     dd 0

    uint8_t driveNumber;            // ebr_drive_number:           db 0
    uint8_t _reserved;              //                             db 0 ; reserved
    uint8_t signature;              // ebr_signature:              db 29h
    uint8_t volumeId[4];            // ebr_volume_id:              db 12h, 34h, 56h, 78h
    uint8_t volumeLabel[11];        // ebr_volume_label:           db 'NANOBYTE OS'        ;=//  11 bytes, padded with spaces
    uint8_t systemId[8];            // ebr_system_id:              db 'FAT12   '           //8 bytes
} __attribute__((packed)) BootSector;

typedef struct {
    uint8_t name[11];
    uint8_t attributes;
    uint8_t _reserved;
    uint8_t createdTimeTenths;
    uint16_t createdTime;
    uint16_t createdDate;
    uint16_t accessedDate;
    uint16_t firstClusterHigh;
    uint16_t modifiedTime;
    uint16_t modifiedDate;
    uint16_t firstClusterLow;
    uint32_t size;
} __attribute__((packed)) DirectoryEntry;


BootSector g_BootSector;
uint8_t* g_Fat = NULL;
DirectoryEntry* g_RootDirectory = NULL;
uint32_t g_RootDirectoryEnd; 

bool readBootSector(FILE* disk){
    return fread(&g_BootSector, sizeof(g_BootSector), 1, disk);
}

bool readSectors(FILE* disk, uint32_t lba, uint32_t count, void* bufferOut){
    bool succeeded = true;
    succeeded = succeeded && (fseek(disk, lba * g_BootSector.bytesPerSector, SEEK_SET) == 0);
    succeeded = succeeded && (fread(bufferOut, g_BootSector.bytesPerSector, count, disk) == count);

    return succeeded;
}

bool readFat(FILE* disk) {
    g_Fat = (uint8_t*) malloc(g_BootSector.sectorsPerFat * g_BootSector.bytesPerSector);
    return readSectors(disk, g_BootSector.reservedSectors, g_BootSector.sectorsPerFat, g_Fat);
}

bool readRootDirectory(FILE* disk){
    uint32_t lba = g_BootSector.reservedSectors + g_BootSector.sectorsPerFat * g_BootSector.fatCount;
    uint32_t size = sizeof(DirectoryEntry) * g_BootSector.dirEntryCount;
    uint32_t sectors = (size / g_BootSector.bytesPerSector);
    if (size % g_BootSector.bytesPerSector > 0) {
        sectors++;
    }

    g_RootDirectoryEnd = lba + sectors;
    g_RootDirectory = (DirectoryEntry*) malloc(sectors * g_BootSector.bytesPerSector);
    return readSectors(disk, lba, sectors, g_RootDirectory);
}

DirectoryEntry* findFile(const char* name) {
    for(uint32_t i = 0; i < g_BootSector.dirEntryCount; i++){
        if (memcmp(name, g_RootDirectory[i].name, 11) == 0){
            return &g_RootDirectory[i];
        }
    }

    return NULL;
}

bool readFile(DirectoryEntry* fileEntry, FILE* disk, uint8_t* outputBuffer) {
    bool success = true;
    uint16_t currentCluster = fileEntry -> firstClusterLow;

    do {
        uint32_t lba = g_RootDirectoryEnd + (currentCluster - 2) * g_BootSector.sectorsPerCluster;
        success = success && readSectors(disk, lba, g_BootSector.sectorsPerCluster, outputBuffer);
        outputBuffer += g_BootSector.sectorsPerCluster * g_BootSector.bytesPerSector;

        uint32_t fatIndex = currentCluster * TWELVE_BITS_CONVERSION;
        printf("G_FAT VALUE: %u", *g_Fat);
        if (currentCluster % 2 == 0) {
            currentCluster = (*(uint16_t*)(g_Fat + fatIndex)) & 0x0FFF;
        } else {
            currentCluster = (*(uint16_t*)(g_Fat + fatIndex)) >> 4;
        }


    } while (success && currentCluster < 0x0FF8);

    return success;
}

int main(int argc, char** argv){ // argv arguments are [0] - disk image [1] - file name
    if (argc < 3) {
        printf("too few arguments there should be 2, [0] - disk image, [1] - file name\n");
        return -1;
    }

    FILE* disk = fopen(argv[1], "rb");

    if(!disk) {
        fprintf(stderr, "Cannot open disk %s\n", argv[1]);
        return -1;
    }

    if(!readBootSector(disk)){
        fprintf(stderr, "Error reading boot sector\n");
        return -2;
    }

    if(!readFat(disk)){
        fprintf(stderr, "Error reading FAT\n");
        free(g_Fat);
        return -3;
    }

    if(!readRootDirectory(disk)){
        fprintf(stderr, "Error reading root directory\n");
        free(g_Fat);
        free(g_RootDirectory);
        return -4;        
    }

    DirectoryEntry* fileEntry = findFile(argv[2]);
    if(!fileEntry){
        fprintf(stderr, "Error no file found\n");
        free(g_Fat);
        free(g_RootDirectory);
        return -5;
    }

    uint8_t* buffer = (uint8_t*) malloc(fileEntry->size + g_BootSector.bytesPerSector);
    if(!readFile(fileEntry, disk, buffer)){
        fprintf(stderr, "Error reading file data\n");
        free(buffer);
        free(g_Fat);
        free(g_RootDirectory);
        return -6;
    }

    for (size_t i = 0; i < fileEntry->size; i++){
        if (isprint(buffer[i])) {
            fputc(buffer[i], stdout);
        } else {
            printf("%02x", buffer[i]);
        }
    }

    free(buffer);
    free(g_Fat);
    free(g_RootDirectory);
    return 0;
}