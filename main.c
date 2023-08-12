#include "hal.h"
#include "fat.h"

void main(int argc, char *argv[])
{
	if ( DISK_OK == HAL_OpenDisk(argv[1]))
	{
		Menu();
	}
}
