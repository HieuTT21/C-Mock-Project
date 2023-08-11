#include "hal.h"
#include "fat.h"

void main()
{
	if ( DISK_OK == HAL_OpenDisk("C://Study//Embedded Develop//C-MockProject//floppy.img"))
	{
		Menu();
	}
}
