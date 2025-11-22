// bmp_stream.h
#ifndef BMP_STREAM_H_
#define BMP_STREAM_H_

#include <stdint.h>
#include "fat_fs.h"

typedef struct {
	FAT_File file;
	uint32_t data_offset;
	uint32_t width;
	uint32_t height;
	uint16_t bpp;
	uint8_t bottom_up;
} BMP_Image;

// Abre un BMP 24-bit sin compresión
uint8_t BMP_Open(BMP_Image *bmp, const char *filename);

// Lee una fila y la convierte a RGB565
// y: 0 = fila superior en pantalla
uint8_t BMP_ReadRow(BMP_Image *bmp, uint32_t y, uint16_t *line_buf);

#endif /* BMP_STREAM_H_ */
