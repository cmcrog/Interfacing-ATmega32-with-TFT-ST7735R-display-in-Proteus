// fat_fs.c - Versión SIMPLE para SD pequeña en FAT (FAT12/16 con root fijo)
// NOTA IMPORTANTE:
//  - Asume archivos NO fragmentados (clústeres contiguos).
//  - Root directory fijo (FAT12/16), como en una SD de 4MB de Proteus.

#include "fat_fs.h"
#include "sd_spi.h"
#include <string.h>
#include <stdint.h>

FAT_Info g_fat;

static uint8_t sector_buffer[512];

// -----------------------------------------------------------------------------
// Leer un sector físico en sector_buffer
// -----------------------------------------------------------------------------
static uint8_t FAT_ReadSector(uint32_t lba)
{
	return SD_ReadBlock(lba, sector_buffer);
}

// -----------------------------------------------------------------------------
// Inicialización FAT (FAT12/16 root fijo)
// -----------------------------------------------------------------------------
uint8_t FAT_Init(void)
{
	// Leer sector 0 (boot sector, sin MBR)
	if (FAT_ReadSector(0) != SD_OK) return 1;

	uint16_t bytes_per_sector    = sector_buffer[11] | ((uint16_t)sector_buffer[12] << 8);
	uint8_t  sectors_per_cluster = sector_buffer[13];
	uint16_t reserved_sectors    = sector_buffer[14] | ((uint16_t)sector_buffer[15] << 8);
	uint8_t  num_fats            = sector_buffer[16];
	uint16_t root_entries        = sector_buffer[17] | ((uint16_t)sector_buffer[18] << 8);
	uint16_t fatsz16             = sector_buffer[22] | ((uint16_t)sector_buffer[23] << 8);

	g_fat.bytes_per_sector    = bytes_per_sector;
	g_fat.sectors_per_cluster = sectors_per_cluster;
	g_fat.root_entry_count    = root_entries;

	uint32_t fat_size = fatsz16;   // en tu SD de 4MB es FAT12/FAT16

	// Sectores que ocupa el root dir
	uint32_t root_dir_sectors =
	((uint32_t)root_entries * 32 + (bytes_per_sector - 1)) / bytes_per_sector;

	g_fat.fat_start_sector  = reserved_sectors;
	g_fat.root_dir_sector   = reserved_sectors + (num_fats * fat_size);
	g_fat.first_data_sector = g_fat.root_dir_sector + root_dir_sectors;

	// No necesitamos distinguir estrictamente FAT12/16 aquí
	g_fat.fat_type = 16; // solo un marcador simbólico

	return 0;
}

// -----------------------------------------------------------------------------
// Utilidad: pasar "NAME.BMP" a nombre 8.3 de 11 bytes en mayúsculas
// -----------------------------------------------------------------------------
static void FAT_MakeName83(const char *name, char out[11])
{
	uint8_t i;
	for (i = 0; i < 11; i++) out[i] = ' ';

	uint8_t j = 0;
	for (i = 0; name[i] != 0 && j < 11; i++) {
		char c = name[i];
		if (c == '.') {
			j = 8;
			} else {
			if (c >= 'a' && c <= 'z') c -= 32;
			out[j++] = c;
		}
	}
}

// -----------------------------------------------------------------------------
// Abrir archivo en el ROOT por nombre 8.3 (p.ej "IMAGE.BMP")
// -----------------------------------------------------------------------------
uint8_t FAT_Open(FAT_File *file, const char *name_8_3)
{
	char target[11];
	FAT_MakeName83(name_8_3, target);

	uint32_t sector = g_fat.root_dir_sector;
	uint16_t entries_per_sector = g_fat.bytes_per_sector / 32;

	// Sectores que ocupa el root dir
	uint32_t root_dir_sectors =
	((uint32_t)g_fat.root_entry_count * 32 + (g_fat.bytes_per_sector - 1)) /
	g_fat.bytes_per_sector;

	uint32_t last_sector = sector + root_dir_sectors;

	for (; sector < last_sector; sector++) {

		if (FAT_ReadSector(sector) != SD_OK) return 1;

		for (uint16_t i = 0; i < entries_per_sector; i++) {
			uint8_t *e = &sector_buffer[i * 32];

			if (e[0] == 0x00) {
				// Fin de directorio
				return 1;
			}
			if (e[0] == 0xE5) {
				// Entrada borrada
				continue;
			}

			uint8_t attr = e[11];
			if (attr & 0x08) continue;   // volume label
			if (attr & 0x10) continue;   // subdirectorio
			if (attr == 0x0F) continue;  // LFN

			if (!memcmp(e, target, 11)) {
				uint16_t first_cluster_low  = e[26] | ((uint16_t)e[27] << 8);
				uint16_t first_cluster_high = e[20] | ((uint16_t)e[21] << 8);
				uint32_t first_cluster      = ((uint32_t)first_cluster_high << 16) | first_cluster_low;
				uint32_t size_bytes         = e[28] | ((uint32_t)e[29] << 8) |
				((uint32_t)e[30] << 16) | ((uint32_t)e[31] << 24);

				file->first_cluster = first_cluster;
				file->size_bytes    = size_bytes;
				file->current_pos   = 0;
				return 0;
			}
		}
	}

	return 1;
}

// -----------------------------------------------------------------------------
// Lectura de archivo (ASUME clústeres contiguos)
// -----------------------------------------------------------------------------
int16_t FAT_Read(FAT_File *file, uint8_t *buffer, uint16_t len)
{
	if (file->current_pos >= file->size_bytes) return 0; // EOF

	if (file->current_pos + len > file->size_bytes) {
		len = file->size_bytes - file->current_pos;
	}

	uint32_t offset = file->current_pos;

	uint32_t bytes_per_sector    = g_fat.bytes_per_sector;
	uint32_t sectors_per_cluster = g_fat.sectors_per_cluster;
	uint32_t cluster_size        = sectors_per_cluster * bytes_per_sector;

	uint32_t cluster_index  = offset / cluster_size;
	uint32_t cluster_offset = offset % cluster_size;

	// SUPOSICIÓN CLAVE: archivo en clústeres contiguos
	uint32_t cluster = file->first_cluster + cluster_index;

	uint32_t first_sector_of_cluster =
	g_fat.first_data_sector + (cluster - 2) * sectors_per_cluster;

	uint32_t sector_in_cluster = cluster_offset / bytes_per_sector;
	uint32_t byte_in_sector    = cluster_offset % bytes_per_sector;

	uint32_t sector_lba = first_sector_of_cluster + sector_in_cluster;

	uint16_t remaining = len;
	uint16_t copied    = 0;

	while (remaining > 0) {

		if (sector_in_cluster >= sectors_per_cluster) {
			// Siguiente clúster contiguo
			cluster++;
			first_sector_of_cluster =
			g_fat.first_data_sector + (cluster - 2) * sectors_per_cluster;

			sector_in_cluster = 0;
			sector_lba        = first_sector_of_cluster;
		}

		if (FAT_ReadSector(sector_lba) != SD_OK) return -1;

		uint16_t can_copy = bytes_per_sector - byte_in_sector;
		if (can_copy > remaining) can_copy = remaining;

		memcpy(&buffer[copied], &sector_buffer[byte_in_sector], can_copy);

		copied      += can_copy;
		remaining   -= can_copy;

		sector_lba++;
		sector_in_cluster++;
		byte_in_sector = 0;
	}

	file->current_pos += copied;
	return copied;
}

// -----------------------------------------------------------------------------
// Utilidades para listar BMPs en el root
// -----------------------------------------------------------------------------

// Convierte nombre FAT (11 bytes) a "NAME.EXT"
static void FAT_Name11ToString(const uint8_t *src11, char *dst13)
{
	uint8_t name_len = 8;
	uint8_t ext_len  = 3;

	while (name_len > 0 && src11[name_len - 1] == ' ')
	name_len--;

	while (ext_len > 0 && src11[8 + ext_len - 1] == ' ')
	ext_len--;

	uint8_t pos = 0;
	for (uint8_t i = 0; i < name_len && pos < 12; i++) {
		dst13[pos++] = src11[i];
	}

	if (ext_len > 0 && pos < 12) {
		dst13[pos++] = '.';
		for (uint8_t i = 0; i < ext_len && pos < 12; i++) {
			dst13[pos++] = src11[8 + i];
		}
	}
	dst13[pos] = '\0';
}

// ¿Tiene extensión .BMP/.bmp?
static uint8_t FAT_HasBmpExt(const char *name)
{
	const char *dot = NULL;
	const char *p = name;
	while (*p) {
		if (*p == '.') dot = p;
		p++;
	}
	if (!dot) return 0;

	if ((dot[1] == 'B' || dot[1] == 'b') &&
	(dot[2] == 'M' || dot[2] == 'm') &&
	(dot[3] == 'P' || dot[3] == 'p') &&
	(dot[4] == '\0'))
	{
		return 1;
	}
	return 0;
}

// Devuelve hasta max_files nombres BMP encontrados en el root
uint8_t FAT_ListBMP(char names[][13], uint8_t max_files)
{
	uint8_t  count = 0;
	uint32_t sector = g_fat.root_dir_sector;
	uint16_t entries_per_sector = g_fat.bytes_per_sector / 32;

	uint32_t root_dir_sectors =
	((uint32_t)g_fat.root_entry_count * 32 + (g_fat.bytes_per_sector - 1)) /
	g_fat.bytes_per_sector;

	uint32_t last_sector = sector + root_dir_sectors;

	for (; sector < last_sector; sector++) {

		if (FAT_ReadSector(sector) != SD_OK)
		return count;

		for (uint16_t i = 0; i < entries_per_sector; i++) {
			uint8_t *e = &sector_buffer[i * 32];

			if (e[0] == 0x00) return count;
			if (e[0] == 0xE5) continue;

			uint8_t attr = e[11];
			if (attr & 0x08) continue;
			if (attr & 0x10) continue;
			if (attr == 0x0F) continue;

			char tmp[13];
			FAT_Name11ToString(e, tmp);
			if (!FAT_HasBmpExt(tmp)) continue;

			if (count < max_files) {
				uint8_t j = 0;
				while (tmp[j] && j < 12) {
					names[count][j] = tmp[j];
					j++;
				}
				names[count][j] = '\0';
				count++;
				} else {
				return count;
			}
		}
	}

	return count;
}
