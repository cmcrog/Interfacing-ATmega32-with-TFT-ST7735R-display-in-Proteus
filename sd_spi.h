// sd_spi.h
#ifndef SD_SPI_H_
#define SD_SPI_H_

#include <stdint.h>

#define SD_OK          0
#define SD_ERR_INIT    1
#define SD_ERR_TIMEOUT 2

// Inicializa la SD en modo SPI.
// Devuelve SD_OK si todo bien.
uint8_t SD_Init(void);

// Lee un bloque (sector) de 512 bytes.
// lba = número de sector lógico (para SDSC, sector = bloque).
// buffer debe ser de 512 bytes.
uint8_t SD_ReadBlock(uint32_t lba, uint8_t *buffer);

#endif /* SD_SPI_H_ */
