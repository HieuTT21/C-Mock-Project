#include <stdio.h>
#include <stdint.h>

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
} __attribute__((packed)) Fat12BootSector;
#pragma pack(pop)

void main()
{
    FILE *fp;
    Fat12BootSector bs;
    char root_dir[4096];

    fp = fopen("floppy.img", "rb");
    if (fp == NULL)
    {
        printf("Error: could not open file.\n");
    }

    fread(&bs, sizeof(Fat12BootSector), 1, fp);
    
    //calculate address of root dir
    uint32_t root_dir_sectors = (bs.root_dir_entries * 32 + bs.bytes_per_sector - 1) / bs.bytes_per_sector;
    uint32_t root_dir_sector = bs.reserved_sectors + bs.number_of_fats * bs.fat_size_sectors;
    uint32_t root_dir_offset = root_dir_sector * bs.bytes_per_sector;
    printf("Root dir address: sector %d, offset %d\n", root_dir_sector, root_dir_offset);

    // calculate address of FAT
    uint32_t fat_sector = bs.reserved_sectors;
    uint32_t fat_offset = fat_sector * bs.bytes_per_sector;
    printf("FAT address: sector %d, offset %d\n", fat_sector, fat_offset);

    fclose(fp);

    printf("Bytes per sector: %d\n", bs.bytes_per_sector);
    printf("Sectors per cluster: %d\n", bs.sectors_per_cluster);
    printf("Number of FATs: %d\n", bs.number_of_fats);
    printf("Root dir entries: %d\n", bs.root_dir_entries);
    printf("Total sectors (short): %d\n", bs.total_sectors_short);
    printf("Media descriptor: %d\n", bs.media_descriptor);
    printf("FAT size (sectors): %d\n", bs.fat_size_sectors);
    printf("Sectors per track: %d\n", bs.sectors_per_track);
    printf("Number of heads: %d\n", bs.number_of_heads);
    printf("Hidden sectors: %d\n", bs.hidden_sectors);
    printf("Total sectors (long): %d\n", bs.total_sectors_long);
    printf("Drive number: %d\n", bs.drive_number);
    printf("Current head: %d\n", bs.current_head);
    printf("Boot signature: %d\n", bs.boot_signature);
    printf("Volume ID: %d\n", bs.volume_id);
    printf("Volume label: %.11s\n", bs.volume_label);
    printf("File system type: %.8s\n", bs.fs_type);


}