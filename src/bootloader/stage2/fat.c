#include "fat.h"
#include "stdio.h"
#include "memdefs.h"
#include "utility.h"
#include "string.h"
#include "memory.h"
#include "ctype.h"

#define min(a,b)    ((a) < (b) ? (a) : (b))
#define max(a,b)    ((a) > (b) ? (a) : (b))

#define SECTOR_SIZE             512
#define MAX_PATH_SIZE           256
#define MAX_FILE_HANDLES        10
#define ROOT_DIRECTORY_HANDLE   -1

#pragma pack(push, 1)

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
    uint8_t volumeLabel[11];        // ebr_volume_label:           db 'MY       OS'        ;=//  11 bytes, padded with spaces
    uint8_t systemId[8];            // ebr_system_id:              db 'FAT12   '           //8 bytes
} FAT_BootSector;

#pragma pack(pop)

typedef struct
{
    uint8_t buffer[SECTOR_SIZE];
    FAT_File public;
    bool opened;
    uint32_t firstCluster;
    uint32_t currentCluster;
    uint32_t currentSectorInCluster;

} FAT_FileData;

typedef struct
{
    union
    {
        FAT_BootSector BootSector;
        uint8_t BootSectorBytes[SECTOR_SIZE];
    } BS;

    FAT_FileData RootDirectory;

    FAT_FileData OpenedFiles[MAX_FILE_HANDLES];

} FAT_Data;

static FAT_Data far* g_Data;
static uint8_t far* g_Fat = NULL;
static uint32_t g_DataSectionLba;

bool FAT_ReadBootSector(DISK* disk){
    return DISK_ReadSectors(disk, 0, 1, g_Data->BS.BootSectorBytes);
}

bool FAT_ReadFat(DISK* disk){
    return DISK_ReadSectors(disk, g_Data->BS.BootSector.reservedSectors, g_Data->BS.BootSector.sectorsPerFat, g_Fat);
}

bool FAT_Initialize(DISK* disk){
    g_Data = (FAT_Data far*)MEMORY_FAT_ADDR;

    // read boot sector
    if (!FAT_ReadBootSector(disk))
    {
        printf("FAT: read boot sector failed\r\n");
        return False;
    }

    // read FAT
    g_Fat = (uint8_t far*)g_Data + sizeof(FAT_Data);
    
    uint32_t fatSize = g_Data->BS.BootSector.bytesPerSector * g_Data->BS.BootSector.sectorsPerFat;
    if (sizeof(FAT_Data) + fatSize >= MEMORY_FAT_SIZE)
    {
        printf("FAT: not enough memory to read FAT!");
        return False;
    }

    if (!FAT_ReadFat(disk))
    {
        printf("FAT: read FAT failed\r\n");
        return False;
    }

    // open root directory file
    uint32_t rootDirLba = g_Data->BS.BootSector.reservedSectors + g_Data->BS.BootSector.sectorsPerFat * g_Data->BS.BootSector.fatCount;
    uint32_t rootDirSize = sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.dirEntryCount;

    g_Data->RootDirectory.public.handle = ROOT_DIRECTORY_HANDLE;
    g_Data->RootDirectory.public.isDirectory = True;
    g_Data->RootDirectory.public.position = 0;
    g_Data->RootDirectory.public.size = sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.dirEntryCount;
    g_Data->RootDirectory.opened = True;
    g_Data->RootDirectory.firstCluster = rootDirLba;
    g_Data->RootDirectory.currentCluster = rootDirLba;
    g_Data->RootDirectory.currentSectorInCluster = 0;

    if (!DISK_ReadSectors(disk, rootDirLba, 1, g_Data->RootDirectory.buffer))
    {
        printf("FAT: read root directory failed\r\n");
        return False;
    }

    // calculate data section
    uint32_t rootDirSectors = (rootDirSize + g_Data->BS.BootSector.bytesPerSector - 1) / g_Data->BS.BootSector.bytesPerSector;
    g_DataSectionLba = rootDirLba + rootDirSectors;

    // reset opened files
    for (int i = 0; i < MAX_FILE_HANDLES; i++)
        g_Data->OpenedFiles[i].opened = False;

    return True;
}

uint32_t FAT_ClusterToLba(uint32_t cluster){
    return g_DataSectionLba + (cluster - 2) * g_Data->BS.BootSector.sectorsPerCluster;
}

FAT_File far* FAT_OpenEntry(DISK* disk, FAT_DirectoryEntry* entry){
    // find empty handle
    int handle = -1;
    for (int i = 0; i < MAX_FILE_HANDLES && handle < 0; i++)
    {
        if (!g_Data->OpenedFiles[i].opened){
            handle = i;
        }
    }

    // out of handles
    if (handle < 0)
    {
        printf("FAT: out of file handles\r\n");
        return False;
    }

    // setup vars
    FAT_FileData far* fd = &g_Data->OpenedFiles[handle];
    fd->public.handle = handle;
    fd->public.isDirectory = (entry->attributes & FAT_ATTRIBUTE_DIRECTORY) != 0;
    fd->public.position = 0;
    fd->public.size = entry->size;
    fd->firstCluster = entry->firstClusterLow + ((uint32_t)entry->firstClusterHigh << 16);
    fd->currentCluster = fd->firstCluster;
    fd->currentSectorInCluster = 0;

    if (!DISK_ReadSectors(disk, FAT_ClusterToLba(fd->currentCluster), 1, fd->buffer))
    {
        printf("FAT: read error\r\n");
        return False;
    }

    fd->opened = True;
    return &fd->public;
}

uint32_t FAT_NextCluster(uint32_t currentCluster){    
    uint32_t fatIndex = currentCluster * 3 / 2;

    if (currentCluster % 2 == 0)
        return (*(uint16_t far*)(g_Fat + fatIndex)) & 0x0FFF;
    else
        return (*(uint16_t far*)(g_Fat + fatIndex)) >> 4;
}

uint32_t FAT_Read(DISK* disk, FAT_File far* file, uint32_t byteCount, void* dataOut){
    // get file data
    FAT_FileData far* fd = (file->handle == ROOT_DIRECTORY_HANDLE) 
        ? &g_Data->RootDirectory 
        : &g_Data->OpenedFiles[file->handle];

    uint8_t* u8DataOut = (uint8_t*)dataOut;

    // don't read past the end of the file
    if (!fd->public.isDirectory) 
        byteCount = min(byteCount, fd->public.size - fd->public.position);

    while (byteCount > 0)
    {
        uint32_t leftInBuffer = SECTOR_SIZE - (fd->public.position % SECTOR_SIZE);
        uint32_t take = min(byteCount, leftInBuffer);

        memcpy(u8DataOut, fd->buffer + fd->public.position % SECTOR_SIZE, take);
        u8DataOut += take;
        fd->public.position += take;
        byteCount -= take;

        // printf("leftInBuffer=%lu take=%lu\r\n", leftInBuffer, take);
        // See if we need to read more data
        if (leftInBuffer == take)
        {
            // Special handling for root directory
            if (fd->public.handle == ROOT_DIRECTORY_HANDLE)
            {
                ++fd->currentCluster;

                // read next sector
                if (!DISK_ReadSectors(disk, fd->currentCluster, 1, fd->buffer))
                {
                    printf("FAT: read error in fat_read in leftInBuffer == take!\r\n");
                    break;
                }
            }
            else
            {
                // calculate next cluster & sector to read
                if (++fd->currentSectorInCluster >= g_Data->BS.BootSector.sectorsPerCluster)
                {
                    fd->currentSectorInCluster = 0;
                    fd->currentCluster = FAT_NextCluster(fd->currentCluster);
                }

                if (fd->currentCluster >= 0xFF8)
                {
                    // Mark end of file
                    fd->public.size = fd->public.position;
                    break;
                }

                // read next sector
                if (!DISK_ReadSectors(disk, FAT_ClusterToLba(fd->currentCluster) + fd->currentSectorInCluster, 1, fd->buffer))
                {
                    printf("FAT: read error in fat_read in leftInBuffer != take!\r\n");
                    break;
                }
            }
        }
    }

    return u8DataOut - (uint8_t*)dataOut;
}

bool FAT_ReadEntry(DISK* disk, FAT_File far* file, FAT_DirectoryEntry* dirEntry){
    return FAT_Read(disk, file, sizeof(FAT_DirectoryEntry), dirEntry) == sizeof(FAT_DirectoryEntry);
}

void FAT_Close(FAT_File far* file){
    if (file->handle == ROOT_DIRECTORY_HANDLE)
    {
        file->position = 0;
        g_Data->RootDirectory.currentCluster = g_Data->RootDirectory.firstCluster;
    }
    else
    {
        g_Data->OpenedFiles[file->handle].opened = False;
    }
}

bool FAT_FindFile(DISK* disk, FAT_File far* file, const char* name, FAT_DirectoryEntry* entryOut){
    char fatName[12];
    FAT_DirectoryEntry entry;

    // convert from name to fat name
    memset(fatName, ' ', sizeof(fatName));
    fatName[11] = '\0';

    const char* ext = strchr(name, '.');
    if (ext == NULL)
        ext = name + 11;

    for (int i = 0; i < 8 && name[i] && name + i < ext; i++)
        fatName[i] = toupper(name[i]);

    if (ext != NULL)
    {
        for (int i = 0; i < 3 && ext[i + 1]; i++)
            fatName[i + 8] = toupper(ext[i + 1]);
    }

    while (FAT_ReadEntry(disk, file, &entry))
    {
        if (memcmp(fatName, entry.name, 11) == 0)
        {
            *entryOut = entry;
            return True;
        }
    }
    
    return False;
}

FAT_File far* FAT_Open(DISK* disk, const char* path){
    char name[MAX_PATH_SIZE];

    // ignore leading slash
    if (path[0] == '/')
        path++;

    FAT_File far* current = &g_Data->RootDirectory.public;

    while (*path) {
        // extract next file name from path
        bool isLast = False;
        const char* delim = strchr(path, '/');
        if (delim != NULL)
        {
            memcpy(name, path, delim - path);
            name[delim - path + 1] = '\0';
            path = delim + 1;
        }
        else
        {
            unsigned len = strlen(path);
            memcpy(name, path, len);
            name[len + 1] = '\0';
            path += len;
            isLast = True;
        }
        
        // find directory entry in current directory
        FAT_DirectoryEntry entry;
        if (FAT_FindFile(disk, current, name, &entry))
        {
            FAT_Close(current);

            // check if directory
            if (!isLast && entry.attributes & FAT_ATTRIBUTE_DIRECTORY == 0)
            {
                printf("FAT: %s not a directory\r\n", name);
                return NULL;
            }

            // open new directory entry
            current = FAT_OpenEntry(disk, &entry);
        }
        else
        {
            FAT_Close(current);

            printf("FAT: %s not found\r\n", name);
            return NULL;
        }
    }

    return current;
}
