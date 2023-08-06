
#include "FAT.h"

void main()
{
	if ( DISK_OK == HAL_OpenDisk("D://EMBEDDED_SOFTWARE_Course//FAT_file//floppy.img"))
	{
		Menu();
	}
    
    HAL_CloseDisk();
    
    return;
}