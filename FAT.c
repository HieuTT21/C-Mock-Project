#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#include "FAT.h"

static FILE *			ptr;
static Fat12BootData 	BootData;
static Fat12Entry 		entry;
DiskStatus_t retStatus = DISK_OK;


static DiskStatus_t HAL_Read_Sector(uint32_t index, Fat12BootData *buffer);
static DiskStatus_t FAT_Read_Directory(uint32_t startSector, uint8_t *numFiles);
static DiskStatus_t FAT_Read_FileContent(uint32_t startCluster);
static DiskStatus_t FAT_Get_StartCluster(uint32_t startSector, uint8_t selectedFile, uint32_t *startCluster);
static void readFat12Entry(uint8_t *entryData, Fat12Entry *entry);
static void convertFilename(uint8_t *name, uint8_t *filename, uint8_t *extension);
static void convertFat12DateTime(uint16_t time, uint16_t date, Fat12DateTime *dateTime);
static void printFat12Entry(Fat12Entry *entry, uint8_t numFiles);
static LinkedList_t *createLinkedList();
static void removeHead(LinkedList_t *list);
static DiskStatus_t addToHead(LinkedList_t *list, uint16_t address);
static void printList(LinkedList_t *list);
static void freeLinkedList(LinkedList_t *list);


DiskStatus_t Menu()
{
	retStatus 					= DISK_OK;
    LinkedList_t *	list 		= createLinkedList();
    uint8_t 		numFiles 	= 0;
    uint32_t 		startCluster;

    if ( READ_SECTOR_OK != HAL_Read_Sector(0, &BootData))
    {
    	return retStatus;
	}
    
    uint32_t rootDirStartSector = BootData.reserved_sectors + (BootData.number_of_fats * BootData.fat_size_sectors);
    
    if ( READ_DIR_OK != FAT_Read_Directory( rootDirStartSector, &numFiles))
    {
    	return retStatus;
	}

    addToHead( list, rootDirStartSector);                  	// Add rootDir address to HEAD
    uint32_t currentAddress = list->head->currentAddress; 	// Get current address value from HEAD

    while (1)
    {
        uint8_t selectedFile = -1;

        while (selectedFile < 0 || selectedFile > numFiles)
        {
            printf("Please select a file or folder (or Press 0 to go back): ");
            scanf(" %d", &selectedFile);
            system("cls");

            if (selectedFile < 0 || selectedFile > numFiles)
            {
                printf("Invalid selection.\n");
                break;
            }

            if (selectedFile == 0)
            {
                if (currentAddress == rootDirStartSector) 		//if user are currently on root dir, break the loop
                {
                    printf("Cant go back further!.\n");
                    break;
                }

                removeHead(list);   							//remove the currentHead when user go back
                currentAddress = list->head->currentAddress;    //Reset currentAddress to previous Head

                FAT_Read_Directory(currentAddress, &numFiles);  //Read and print the previous directory
            }
            else
            {
                FAT_Get_StartCluster(currentAddress, selectedFile, &startCluster); // Pass current Address of HEAD to search selected entry of the current folder to get start cluster

                addToHead(list, startCluster); 					// Add the start cluster of selected entry to HEAD

                currentAddress = list->head->currentAddress; 	// Reset currentAddress to new current HEAD

                if (entry.isDirectory)
                {
                    FAT_Read_Directory(currentAddress, &numFiles); // if its a folder, pass current cluster to read all its entries
                }
                else
                {
                    FAT_Read_FileContent(currentAddress); 			// if its a file, pass current cluster to read all its content
                    numFiles = 0;
                }
            }
        }
    }
    
    return retStatus;
}

static LinkedList_t *createLinkedList()
{
    LinkedList_t * newLinkedList = (LinkedList_t *)malloc(sizeof(LinkedList_t));
    if ( NULL == newLinkedList);
    {
    	printf("Error: Memory Allocation !");
	}
    newLinkedList->head	= NULL;
    
    return newLinkedList;
}

static void removeHead(LinkedList_t *list)
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

static DiskStatus_t addToHead(LinkedList_t *list, uint16_t address)
{
    Node_t * newHead = (Node_t *)malloc(sizeof(Node_t));
    if ( NULL == newHead)
    {
    	retStatus = ALLOCATE_MEM_ERR;
	}
	else
	{
		newHead->currentAddress = address;
	    newHead->next = list->head;
	    list->head = newHead;
	}
    
    return retStatus;
}

static void printList(LinkedList_t *list)
{
    Node_t * currentNode = list->head;
    while (currentNode != NULL)
    {
        printf("address: %d\t", currentNode->currentAddress);
        currentNode = currentNode->next;
    }
}

static void freeLinkedList(LinkedList_t *list)
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
DiskStatus_t HAL_OpenDisk(const uint8_t *DiskPath)
{
	retStatus = DISK_OK;
    ptr = fopen(DiskPath, "rb");

    if (ptr == NULL)
    {
    	retStatus = DISK_ERR_OPEN;
        printf("Error: could not open file.\n");
    }
    
    return retStatus;
}

// Close Disk
DiskStatus_t HAL_CloseDisk(void)
{
	if ( 0 != fclose( ptr))
	{
		retStatus = DISK_ERR_CLOSE;
	}
	
	return retStatus;
}

// Read a sector
static DiskStatus_t HAL_Read_Sector(uint32_t index, Fat12BootData *buffer)
{
	retStatus = READ_SECTOR_OK;
    uint32_t sector_offset = index * SECTOR_SIZE;

    if ( 0 != fseek(ptr, sector_offset, SEEK_SET))
    {
    	retStatus = SEEK_SECTOR_ERR;
    	printf("Error: Could not seek sector !");
    	return retStatus;
	}
	
	if ( SECTOR_SIZE != fread(buffer, 1, SECTOR_SIZE, ptr))
	{
		retStatus = READ_SECTOR_ERR;
		printf("Error: Read Sector !");
	}
    
    return retStatus;
}

// Read Directory
DiskStatus_t FAT_Read_Directory(uint32_t startSector, uint8_t *numFiles)
{
	retStatus = READ_DIR_OK;
    uint8_t buffer[SECTOR_SIZE];
    uint8_t countNumFiles = 0;
    uint32_t j;
	
	if ( READ_SECTOR_OK == HAL_Read_Sector(startSector, (Fat12BootData*)buffer))
	{
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

        	readFat12Entry( &buffer[j], &entry);
        	uint8_t filename[MAX_FILENAME_LENGTH + MAX_EXTENSION_LENGTH + 2];
        	convertFilename( &buffer[j], filename, entry.extension);

        	printFat12Entry(&entry, countNumFiles);
    	}

    	(*numFiles) = countNumFiles;
    	retStatus = READ_DIR_OK;
	}
	else
	{
		retStatus = READ_DIR_ERR;
		printf("Error: Read Directory !");
	}

    return retStatus;
}

// Read File content
DiskStatus_t FAT_Read_FileContent( uint32_t startCluster)
{
	retStatus = READ_CONTENT_OK;
    uint8_t buffer[SECTOR_SIZE];
	
	if ( 0 == fseek(ptr, startCluster * SECTOR_SIZE, SEEK_SET))
	{
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
	else 
	{
		retStatus = SEEK_CONTENT_ERR;
		printf("Error: Read File Content !");
	}

    return retStatus;
}

// Get Start Cluster
DiskStatus_t FAT_Get_StartCluster( uint32_t startSector, uint8_t selectedFile, uint32_t *startCluster)
{
	retStatus 	  = GET_STARTCLUSTER_OK;
	uint32_t 	i = 0;
    uint32_t 	j = 0;
    uint8_t 	buffer[SECTOR_SIZE];
    
	
	if ( READ_SECTOR_OK == HAL_Read_Sector( startSector, (Fat12BootData*)buffer))
	{
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
	else 
	{
		retStatus = GET_STARTCLUSTER_ERR;
		printf("Error: Get Start Cluster !");
	}
	
	return retStatus;    
}

//  Get data in each entry
static void readFat12Entry(uint8_t *entryData, Fat12Entry *entry)
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

static void convertFat12DateTime(uint16_t date, uint16_t time, Fat12DateTime *dateTime)
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

static void convertFilename(uint8_t *name, uint8_t *filename, uint8_t *extension)
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

static void printFat12Entry(Fat12Entry *entry, uint8_t numFiles)
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