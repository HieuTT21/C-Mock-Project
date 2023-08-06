#ifndef _FAT_H_
#define _FAT_H_

#include <stdint.h>
#include <stdbool.h>

#define SECTOR_SIZE 			(0x200)
#define ENTRY_SIZE				(0x20)
#define MAX_FILENAME_LENGTH 	(0x08)
#define MAX_EXTENSION_LENGTH 	(0x03)
#define ATTRIBUTE_BYTE			(0x0b)

typedef struct Node
{
    uint32_t 		currentAddress;
    struct Node *	next;
} Node_t;

typedef struct
{
    Node_t * head;
} LinkedList_t;

#pragma pack(push, 1)               // exact fit - no padding
typedef struct
{
    uint8_t 	jmp[3];
    uint8_t		oem[8];
    uint16_t 	bytes_per_sector;
    uint8_t 	sectors_per_cluster;
    uint16_t 	reserved_sectors;
    uint8_t 	number_of_fats;
    uint16_t 	root_dir_entries;
    uint16_t 	total_sectors_short; // if zero, later field is used
    uint8_t 	media_descriptor;
    uint16_t 	fat_size_sectors;
    uint16_t 	sectors_per_track;
    uint16_t 	number_of_heads;
    uint32_t 	hidden_sectors;
    uint32_t 	total_sectors_long;

    // used by FAT12 and FAT16
    uint8_t 	drive_number;
    uint8_t 	current_head;
    uint8_t 	boot_signature;
    uint32_t 	volume_id;
    uint8_t 	volume_label[11];
    uint8_t 	fs_type[8]; 		// typically contains "FAT12   "
} __attribute__((packed)) Fat12BootData;
#pragma pack(pop)

typedef struct
{
    uint16_t 	year;
    uint8_t 	month;
    uint8_t 	day;
    uint8_t 	hour;
    uint8_t 	minute;
    uint8_t 	second;
} Fat12DateTime;

typedef struct
{
    uint8_t 		filename[MAX_FILENAME_LENGTH + 1];   // +1 for null terminator
    uint8_t 		extension[MAX_EXTENSION_LENGTH + 1]; // +1 for null terminator
    uint8_t 		attributes;
    Fat12DateTime 	creationTime;
    Fat12DateTime 	lastAccessTime;
    Fat12DateTime 	lastModifiedTime;
    uint16_t 		startCluster;
    uint32_t 		fileSize;
    bool 			isDirectory;
} Fat12Entry;

typedef enum
{
	DISK_OK                 = 0x00,
	DISK_ERR_OPEN           = 0x01,
	DISK_ERR_CLOSE          = 0x02,
	READ_SECTOR_OK          = 0x03,
	SEEK_SECTOR_ERR         = 0x04,
	READ_SECTOR_ERR         = 0x05,
	READ_DIR_OK             = 0x06,
	READ_DIR_ERR            = 0x07,
	READ_CONTENT_OK         = 0x08,
	SEEK_CONTENT_ERR        = 0x09,
	GET_STARTCLUSTER_OK     = 0x0a,
	GET_STARTCLUSTER_ERR    = 0x0b,
	ALLOCATE_MEM_ERR        = 0x0c	
} DiskStatus_t;

// Global Prototype function
DiskStatus_t Menu();
DiskStatus_t HAL_OpenDisk( const uint8_t *);
DiskStatus_t HAL_CloseDisk();

#endif