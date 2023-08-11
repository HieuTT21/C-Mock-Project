#include "hal.h"
#include "fat.h"
#include "linkedlist.h"

static Fat12BootData 	BootData;
static Fat12Entry 		entry;

static DiskStatus_t FAT_Read_Directory(uint32_t startSector, uint8_t *numFiles);
static DiskStatus_t FAT_Read_FileContent(uint32_t startCluster);
static DiskStatus_t FAT_Get_StartCluster(uint32_t startSector, uint8_t selectedFile, uint32_t *startCluster);
static void readFat12Entry(uint8_t *entryData, Fat12Entry *entry);
static void convertFat12DateTime(uint16_t time, uint16_t date, Fat12DateTime *dateTime);
static void printFat12Entry(Fat12Entry *entry, uint8_t numFiles);


DiskStatus_t Menu()
{
	DiskStatus_t retStatus 		= DISK_OK;
    LinkedList_t *	list 		= createLinkedList();
    uint8_t 		numFiles 	= 0;
    uint32_t 		startCluster;

    if ( READ_SECTOR_OK != HAL_Read_Sector(0, &BootData))
    {
    	return retStatus;
	}
    
    printf("FAT Type: %.5s\n", BootData.fs_type);
    
    uint32_t rootDirStartSector = BootData.reserved_sectors + (BootData.number_of_fats * BootData.fat_size_sectors);
    
    if ( READ_DIR_OK != FAT_Read_Directory( rootDirStartSector, &numFiles))
    {
    	return retStatus;
	}

    addToHead(list, rootDirStartSector);                  	// Add rootDir address to HEAD
    uint32_t currentAddress = list->head->currentAddress; 	// Get current address value from HEAD

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
            	system("cls");
                if (currentAddress == rootDirStartSector) 
				{		                                       	//if user are currently on root dir, exit the program
                    printf("Exiting Program!\n");
                    freeLinkedList(list);
                    HAL_CloseDisk();
                    exit(0);
                }

                removeHead(list);   							//remove the currentHead when user go back
                currentAddress = list->head->currentAddress;    //Reset currentAddress to previous Head

                FAT_Read_Directory(currentAddress, &numFiles);  //Read and print the previous directory
            }
            else
            {
            	system("cls");
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

// Read Directory
DiskStatus_t FAT_Read_Directory(uint32_t startSector, uint8_t *numFiles)
{
	printf("\n");
	
	DiskStatus_t retStatus = READ_DIR_OK;
    uint8_t buffer[SECTOR_SIZE];
    
    printf("| Filename |\t| Extension |\t| Creation Date |\t| Last Access Date |\t| Last Write Date |\t| File Size |\n\n");
    
    uint8_t countNumFiles = 0;
    uint32_t j;
	
	if ( READ_SECTOR_OK == HAL_Read_Sector(startSector, buffer))
	{
		for (j = 0; j < SECTOR_SIZE; j += ENTRY_SIZE)
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

        	printFat12Entry(&entry, countNumFiles);
    	}

    	(*numFiles) = countNumFiles;
    	retStatus = READ_DIR_OK;
    	
    	if (buffer[0] == 0x2E && buffer[33] == 0x2E && j <= 64)
    	{
        	printf("This Folder is empty. Please go back.\n");
    	}
	}
	else
	{
		retStatus = READ_DIR_ERR;
		printf("Error: Read Directory !");
	}

	printf("\n");

    return retStatus;
}

// Read File content
DiskStatus_t FAT_Read_FileContent( uint32_t startCluster)
{
	printf("\n");
	
	if (entry.fileSize == 0)
    {
        printf("This file is empty. Please go back.\n");
        return;
    }
	
	DiskStatus_t retStatus = READ_CONTENT_OK;
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
	DiskStatus_t retStatus 	  = GET_STARTCLUSTER_OK;
	uint32_t 	i = 0;
    uint32_t 	j = 0;
    uint8_t 	buffer[SECTOR_SIZE];
    
	
	if ( READ_SECTOR_OK == HAL_Read_Sector( startSector, (Fat12BootData*)buffer))
	{
		while (j < selectedFile)
	    {
	        if (buffer[i + 11] == 0x0F || buffer[i] == 0xE5 || buffer[i] == 0x2E)
	        {
	            i += ENTRY_SIZE;
	            continue;
	        }
	
	        j++;
	
	        if (j == selectedFile)
	        {
	            readFat12Entry(&buffer[i], &entry);
	            break;
	        }
	
	        i += ENTRY_SIZE;
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

static void printFat12Entry(Fat12Entry *entry, uint8_t numFiles)
{
    printf("%d/ ", numFiles);
    printf("%s\t", entry->filename);
    if (entry->attributes & 0x10)
    {
        printf("FILE FOLDER\t");
    }
    else
    {
        printf("FILE %s\t", entry->extension);
    }
    printf("%04d-%02d-%02d %02d:%02d:%02d\t",
           entry->creationTime.year, entry->creationTime.month, entry->creationTime.day,
           entry->creationTime.hour, entry->creationTime.minute, entry->creationTime.second);
    printf("%04d-%02d-%02d %02d:%02d:%02d\t",
           entry->lastAccessTime.year, entry->lastAccessTime.month, entry->lastAccessTime.day,
           entry->lastAccessTime.hour, entry->lastAccessTime.minute, entry->lastAccessTime.second);
    printf("%04d-%02d-%02d %02d:%02d:%02d\t",
           entry->lastModifiedTime.year, entry->lastModifiedTime.month, entry->lastModifiedTime.day,
           entry->lastModifiedTime.hour, entry->lastModifiedTime.minute, entry->lastModifiedTime.second);
    if (entry->fileSize != 0)
    {
        printf("%lub\n", entry->fileSize);
    }
    else
    {
        printf("\n");
    }
}
