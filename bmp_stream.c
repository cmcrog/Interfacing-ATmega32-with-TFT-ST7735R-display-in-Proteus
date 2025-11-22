// bmp_stream.c
#include "bmp_stream.h"
#include <string.h>

uint8_t BMP_Open(BMP_Image *bmp, const char *filename)
{
	if (FAT_Open(&bmp->file, filename) != 0) return 1;

	uint8_t header[54];
	if (FAT_Read(&bmp->file, header, 54) != 54) return 2;

	if (header[0] != 'B' || header[1] != 'M') return 3; // no es BMP

	uint32_t data_offset = header[10] | ((uint32_t)header[11]<<8) |
	((uint32_t)header[12]<<16) | ((uint32_t)header[13]<<24);
	uint32_t width = header[18] | ((uint32_t)header[19]<<8) |
	((uint32_t)header[20]<<16) | ((uint32_t)header[21]<<24);
	uint32_t height = header[22] | ((uint32_t)header[23]<<8) |
	((uint32_t)header[24]<<16) | ((uint32_t)header[25]<<24);
	uint16_t bpp = header[28] | ((uint16_t)header[29]<<8);

	if (bpp != 24) return 4; // solo soportamos 24-bit por ahora

	bmp->data_offset = data_offset;
	bmp->width = width;
	bmp->height = (height < 0x80000000UL) ? height : (0xFFFFFFFF - height + 1);
	bmp->bpp = bpp;
	bmp->bottom_up = (height > 0); // BMP típico es bottom-up

	// Volver el puntero de archivo al inicio de los datos
	bmp->file.current_pos = data_offset;
	return 0;
}

uint8_t BMP_ReadRow(BMP_Image *bmp, uint32_t y, uint16_t *line_buf)
{
	uint32_t row_index = y;
	if (bmp->bottom_up) {
		row_index = bmp->height - 1 - y;
	}

	uint32_t row_size_bytes = ((bmp->width * 3 + 3) / 4) * 4; // alineado a 4
	uint32_t offset = bmp->data_offset + row_index * row_size_bytes;

	// Mover puntero de archivo a offset deseado
	bmp->file.current_pos = offset;

	static uint8_t row_buf[512]; // suficiente para widths pequeñas; para 128x160 ok (128*3=384)
	if (row_size_bytes > sizeof(row_buf)) return 1; // muy grande para este ejemplo

	if (FAT_Read(&bmp->file, row_buf, row_size_bytes) != (int16_t)row_size_bytes) return 2;

	// Convertir BGR888 a RGB565
	for (uint32_t x=0; x<bmp->width; x++) {
		uint8_t b = row_buf[x*3 + 0];
		uint8_t g = row_buf[x*3 + 1];
		uint8_t r = row_buf[x*3 + 2];

		uint16_t r5 = (r >> 3) & 0x1F;
		uint16_t g6 = (g >> 2) & 0x3F;
		uint16_t b5 = (b >> 3) & 0x1F;

		line_buf[x] = (r5 << 11) | (g6 << 5) | b5;
	}

	return 0;
}
