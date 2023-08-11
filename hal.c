#include "hal.h"
#include "fat.h"

DiskStatus_t HAL_OpenDisk(const uint8_t *DiskPath)
{
	DiskStatus_t retStatus = DISK_OK;
    ptr = fopen(DiskPath, "rb");

    if (ptr == NULL)
    {
    	retStatus = DISK_ERR_OPEN;
        printf("Error: could not open file.\n");
    }
    
    return retStatus;
}

DiskStatus_t HAL_CloseDisk(void)
{
	DiskStatus_t retStatus = DISK_OK;
	if ( 0 != fclose( ptr))
	{
		retStatus = DISK_ERR_CLOSE;
	}
	
	return retStatus;
}

DiskStatus_t HAL_Read_Sector(uint32_t index, uint8_t *buffer)
{
	DiskStatus_t retStatus = READ_SECTOR_OK;
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
