#ifndef __HAL_H__
#define __HAL_H__

#include <stdint.h>
#include <stdio.h>

#define SECTOR_SIZE 			(0x200)
#define ENTRY_SIZE				(0x20)
#define MAX_FILENAME_LENGTH 	(0x08)
#define MAX_EXTENSION_LENGTH 	(0x03)
#define ATTRIBUTE_BYTE			(0x0b)

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

FILE * ptr;


DiskStatus_t HAL_OpenDisk(const uint8_t *DiskPath);
DiskStatus_t HAL_CloseDisk(void);
DiskStatus_t HAL_Read_Sector(uint32_t index, uint8_t *buffer);

#endif
