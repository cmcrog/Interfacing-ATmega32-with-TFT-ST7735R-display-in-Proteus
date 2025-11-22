// fat_fs.h
#ifndef FAT_FS_H_
#define FAT_FS_H_

#include <stdint.h>
#include "sd_spi.h"

typedef struct {
	uint32_t first_data_sector;
	uint32_t root_dir_sector;
	uint16_t bytes_per_sector;
	uint8_t  sectors_per_cluster;
	uint32_t fat_start_sector;
	uint32_t root_entry_count;
	uint8_t  fat_type; // 16 o 32
} FAT_Info;

typedef struct {
	uint32_t first_cluster;
	uint32_t size_bytes;
	uint32_t current_pos;
} FAT_File;

extern FAT_Info g_fat;

uint8_t FAT_Init(void);
uint8_t FAT_Open(FAT_File *file, const char *name_8_3); // nombre 8.3 en mayúsculas
int16_t FAT_Read(FAT_File *file, uint8_t *buffer, uint16_t len);

#endif /* FAT_FS_H_ */
