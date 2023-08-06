#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define SECTOR_SIZE (512U)
#define MAX_FILENAME_LENGTH 8
#define MAX_EXTENSION_LENGTH 3

typedef struct Node
{
    uint32_t currentAddress;
    struct Node *next;
} Node_t;

typedef struct
{
    Node_t *head;
} LinkedList_t;

#pragma pack(push, 1) // exact fit - no padding
typedef struct
{
    uint8_t jmp[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t number_of_fats;
    uint16_t root_dir_entries;
    uint16_t total_sectors_short; // if zero, later field is used
    uint8_t media_descriptor;
    uint16_t fat_size_sectors;
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_long;

    // used by FAT12 and FAT16
    uint8_t drive_number;
    uint8_t current_head;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8]; // typically contains "FAT12   "
} __attribute__((packed)) Fat12BootData;
#pragma pack(pop)

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} Fat12DateTime;

typedef struct
{
    uint8_t filename[MAX_FILENAME_LENGTH + 1];   // +1 for null terminator
    uint8_t extension[MAX_EXTENSION_LENGTH + 1]; // +1 for null terminator
    uint8_t attributes;
    Fat12DateTime creationTime;
    Fat12DateTime lastAccessTime;
    Fat12DateTime lastModifiedTime;
    uint16_t startCluster;
    uint32_t fileSize;
    bool isDirectory;
} Fat12Entry;

static FILE *ptr;
static Fat12BootData BootData;
Fat12Entry entry;

void Menu();
void readFat12Entry(uint8_t *entryData, Fat12Entry *entry);
void convertFilename(uint8_t *name, uint8_t *filename, uint8_t *extension);
void convertFat12DateTime(uint16_t time, uint16_t date, Fat12DateTime *dateTime);
void printFat12Entry(Fat12Entry *entry, uint8_t numFiles);
void FAT_Read_Directory(uint32_t startSector, uint8_t *numFiles);
void FAT_Read_FileContent(uint32_t startCluster);
void FAT_Get_StartCluster(uint32_t startSector, uint8_t selectedFile, uint32_t *startCluster);

LinkedList_t *createLinkedList()
{
    LinkedList_t *newLinkedList = (LinkedList_t *)malloc(sizeof(LinkedList_t));
    newLinkedList->head = NULL;
    return newLinkedList;
}

void removeHead(LinkedList_t *list)
{
    if (list->head == NULL)
    {
        printf("List is empty\n");
        return;
    }
    Node_t *temp = list->head;
    list->head = list->head->next;
    free(temp);
}

void addToHead(LinkedList_t *list, uint16_t address)
{
    Node_t *newHead = (Node_t *)malloc(sizeof(Node_t));
    newHead->currentAddress = address;
    newHead->next = list->head;
    list->head = newHead;
}

void printList(LinkedList_t *list)
{
    Node_t *currentNode = list->head;
    while (currentNode != NULL)
    {
        printf("address: %d\t", currentNode->currentAddress);
        currentNode = currentNode->next;
    }
}

void freeLinkedList(LinkedList_t *list)
{
    Node_t *currentHead = list->head;
    while (currentHead != NULL)
    {
        Node_t *nextNode = currentHead->next;
        free(currentHead);
        currentHead = nextNode;
    }
    free(list);
}

// Open Disk
void HAL_OpenDisk(const uint8_t *DiskPath)
{
    ptr = fopen(DiskPath, "rb");

    if (ptr == NULL)
    {
        printf("Error: could not open file.\n");
    }
}

// Close Disk
void HAL_CloseDisk(void)
{
    fclose(ptr);
}

void HAL_Read_Sector(uint32_t index, uint8_t *buffer)
{
    uint32_t sector_offset = index * SECTOR_SIZE;

    fseek(ptr, sector_offset, SEEK_SET);
    fread(buffer, 1, SECTOR_SIZE, ptr);
}

void FAT_Read_Directory(uint32_t startSector, uint8_t *numFiles)
{
    uint8_t buffer[SECTOR_SIZE];

    HAL_Read_Sector(startSector, buffer);

    uint8_t countNumFiles = 0;
    uint32_t j;
    for (j = 0; j < SECTOR_SIZE; j += 32)
    {
        if (buffer[j] == 0x2E)
        {
            continue;
        }

        if (buffer[j] == 0x00)
        {
            // End of directory entries
            break;
        }

        if (buffer[j] == 0xE5)
        {
            // Unused entry
            continue;
        }

        if (buffer[j + 11] == 0x0F)
        {
            // Long filename entry
            continue;
        }

        countNumFiles++;

        readFat12Entry(&buffer[j], &entry);
        uint8_t filename[MAX_FILENAME_LENGTH + MAX_EXTENSION_LENGTH + 2];
        convertFilename(&buffer[j], filename, entry.extension);

        printFat12Entry(&entry, countNumFiles);
    }

    (*numFiles) = countNumFiles;
}

void FAT_Read_FileContent(uint32_t startCluster)
{
    uint8_t buffer[SECTOR_SIZE];

    fseek(ptr, startCluster * SECTOR_SIZE, SEEK_SET);

    while (entry.fileSize > 0)
    {
        uint32_t bytesRead = fread(buffer, sizeof(uint8_t), SECTOR_SIZE, ptr);
        if (bytesRead == 0)
        {
            printf("End of File\n");
            break;
        }

        // Print the buffer content to the console
        uint32_t printSize = (entry.fileSize < bytesRead) ? entry.fileSize : bytesRead;

        uint32_t i;
        for (i = 0; i < printSize; i++)
        {
            printf("%c", buffer[i]);
        }
        entry.fileSize -= printSize;

        if (entry.fileSize == 0)
            break;
    }
    printf("\n");
}

void FAT_Get_StartCluster(uint32_t startSector, uint8_t selectedFile, uint32_t *startCluster)
{
    uint8_t buffer[SECTOR_SIZE];

    HAL_Read_Sector(startSector, buffer);

    uint32_t i = 0;
    uint32_t j = 0;

    while (j < selectedFile)
    {
        if (buffer[i + 11] == 0x0F || buffer[i] == 0xE5 || buffer[i] == 0x2E)
        {
            i += 32;
            continue;
        }

        j++;

        if (j == selectedFile)
        {
            readFat12Entry(&buffer[i], &entry);
            break;
        }

        i += 32;
    }

    if (entry.attributes & 0x10)
    {
        entry.isDirectory = true;
    }
    else
    {
        entry.isDirectory = false;
    }

    uint32_t data_region_start_sector = BootData.reserved_sectors + (BootData.number_of_fats * BootData.fat_size_sectors) + (BootData.root_dir_entries * 32 / BootData.bytes_per_sector);
    (*startCluster) = ((entry.startCluster - 2) * BootData.sectors_per_cluster) + data_region_start_sector;
}

void Menu()
{
    LinkedList_t *list = createLinkedList();

    uint8_t numFiles = 0;
    uint32_t startCluster;

    HAL_OpenDisk("C://Study//Embedded Develop//C-MockProject//floppy.img");

    HAL_Read_Sector(0, &BootData);

    uint32_t rootDirStartSector = BootData.reserved_sectors + (BootData.number_of_fats * BootData.fat_size_sectors);
    // Read RootDir
    FAT_Read_Directory(rootDirStartSector, &numFiles);

    addToHead(list, rootDirStartSector);                  // Add rootDir address to HEAD
    uint32_t currentAddress = list->head->currentAddress; // Get current address value from HEAD

    while (1)
    {
        uint8_t selectedFile = -1;

        while (selectedFile < 0 || selectedFile > numFiles)
        {
            printf("Please select a file or folder (or Press 0 to go back): ");
            scanf(" %d", &selectedFile);

            if (selectedFile < 0 || selectedFile > numFiles)
            {
                printf("Invalid selection.\n");
                break;
            }

            if (selectedFile == 0)
            {
                if (currentAddress == rootDirStartSector) //if user are currently on root dir, break the loop
                {
                    printf("Cant go back further!.\n");
                    break;
                }

                removeHead(list);   //remove the currentHead when user go back
                currentAddress = list->head->currentAddress;    //Reset currentAddress to previous Head

                FAT_Read_Directory(currentAddress, &numFiles);  //Read and print the previous directory
            }
            else
            {
                FAT_Get_StartCluster(currentAddress, selectedFile, &startCluster); // Pass current Address of HEAD to search selected entry of the current folder to get start cluster

                addToHead(list, startCluster); // Add the start cluster of selected entry to HEAD

                currentAddress = list->head->currentAddress; // Reset currentAddress to new current HEAD

                if (entry.isDirectory)
                {
                    FAT_Read_Directory(currentAddress, &numFiles); // if its a folder, pass current cluster to read all its entries
                }
                else
                {
                    FAT_Read_FileContent(currentAddress); // if its a file, pass current cluster to read all its content
                }
            }
        }
    }

    HAL_CloseDisk();
}

//  Get data in each entry
void readFat12Entry(uint8_t *entryData, Fat12Entry *entry)
{
    // Copy the filename and extension
    memcpy(entry->filename, entryData, MAX_FILENAME_LENGTH);
    entry->filename[MAX_FILENAME_LENGTH] = '\0';
    memcpy(entry->extension, entryData + 8, MAX_EXTENSION_LENGTH);
    entry->extension[MAX_EXTENSION_LENGTH] = '\0';

    // Copy the attributes, creation time, creation date, last access date, last modified time, last modified date, and start cluster
    entry->attributes = entryData[11];
    convertFat12DateTime(*(uint16_t *)(entryData + 16), *(uint16_t *)(entryData + 14), &entry->creationTime);
    convertFat12DateTime(*(uint16_t *)(entryData + 18), 0, &entry->lastAccessTime);
    convertFat12DateTime(*(uint16_t *)(entryData + 24), *(uint16_t *)(entryData + 22), &entry->lastModifiedTime);
    entry->startCluster = *(uint16_t *)(entryData + 26);

    // Copy the file size
    entry->fileSize = *(uint32_t *)(entryData + 28);
}

void convertFat12DateTime(uint16_t date, uint16_t time, Fat12DateTime *dateTime)
{
    // Extract the year, month, and day from the date
    dateTime->year = ((date >> 9) & 0x7f) + 1980;
    dateTime->month = (date >> 5) & 0x0f;
    dateTime->day = date & 0x1f;

    // Extract the hour, minute, and second from the time
    dateTime->hour = (time >> 11) & 0x1f;
    dateTime->minute = (time >> 5) & 0x3f;
    dateTime->second = (time & 0x1f) * 2;
}

void convertFilename(uint8_t *name, uint8_t *filename, uint8_t *extension)
{
    // Convert the filename to a null-terminated string
    uint32_t i;
    uint32_t j;

    for (i = 0; i < MAX_FILENAME_LENGTH && name[i] != ' '; i++)
    {
        filename[i] = name[i];
    }
    filename[i] = '\0';

    // Convert the extension to a null-terminated string

    for (j = 0; j < MAX_EXTENSION_LENGTH && extension[j] != ' '; j++)
    {
        extension[j] = extension[j];
    }
    extension[j] = '\0';

    // If the extension is not empty, append it to the filename
    if (extension[0] != '\0')
    {
        strcat(filename, ".");
        strcat(filename, extension);
    }
}

void printFat12Entry(Fat12Entry *entry, uint8_t numFiles)
{
    printf("%d/ ", numFiles);
    printf("%s.%s\t", entry->filename, entry->extension);
    printf("%c%c%c%c%c\t",
           (entry->attributes & 0x10) ? 'D' : '-',
           (entry->attributes & 0x01) ? 'R' : '-',
           (entry->attributes & 0x02) ? 'H' : '-',
           (entry->attributes & 0x04) ? 'S' : '-',
           (entry->attributes & 0x08) ? 'A' : '-');
    printf("%04d-%02d-%02d %02d:%02d:%02d\t",
           entry->creationTime.year, entry->creationTime.month, entry->creationTime.day,
           entry->creationTime.hour, entry->creationTime.minute, entry->creationTime.second);
    printf("%04d-%02d-%02d %02d:%02d:%02d\t",
           entry->lastAccessTime.year, entry->lastAccessTime.month, entry->lastAccessTime.day,
           entry->lastAccessTime.hour, entry->lastAccessTime.minute, entry->lastAccessTime.second);
    printf("%04d-%02d-%02d %02d:%02d:%02d\t",
           entry->lastModifiedTime.year, entry->lastModifiedTime.month, entry->lastModifiedTime.day,
           entry->lastModifiedTime.hour, entry->lastModifiedTime.minute, entry->lastModifiedTime.second);
    printf("%x\t", entry->startCluster);
    if (entry->fileSize != 0)
    {
        printf("%lub\n", entry->fileSize);
    }
    else
    {
        printf("\n");
    }
}

void main()
{
    Menu();

    // printf("Bytes per sector: %d\n", BootData.bytes_per_sector);
    // printf("Sectors per cluster: %d\n", BootData.sectors_per_cluster);
    // printf("Reserved sectors: %d\n", BootData.reserved_sectors);
    // printf("Number of FATs: %d\n", BootData.number_of_fats);
    // printf("Root dir entries: %d\n", BootData.root_dir_entries);
    // printf("Total sectors (short): %d\n", BootData.total_sectors_short);
    // printf("Media descriptor: %d\n", BootData.media_descriptor);
    // printf("FAT size (sectors): %d\n", BootData.fat_size_sectors);
    // printf("Sectors per track: %d\n", BootData.sectors_per_track);
    // printf("Number of heads: %d\n", BootData.number_of_heads);
    // printf("Hidden sectors: %d\n", BootData.hidden_sectors);
    // printf("Total sectors (long): %d\n", BootData.total_sectors_long);
    // printf("Drive number: %d\n", BootData.drive_number);
    // printf("Current head: %d\n", BootData.current_head);
    // printf("Boot signature: %d\n", BootData.boot_signature);
    // printf("Volume ID: %d\n", BootData.volume_id);
    // printf("Volume label: %.11s\n", BootData.volume_label);
    // printf("File system type: %.8s\n", BootData.fs_type);

    // data_region_start_sector = BootData.reserved_sectors + (BootData.number_of_fats * BootData.fat_size_sectors) + (BootData.root_dir_entries * 32 / BootData.bytes_per_sector);
    // sector = ((cluster - 2) * BootData.sectors_per_cluster) + data_region_start_sector;
}