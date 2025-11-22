// sd_spi.c
#include "sd_spi.h"
#include "spi_hal.h"

#define SD_TOKEN_START_BLOCK  0xFE

// Envía múltiples clocks con MOSI=1 para “despertar” la SD
static void SD_SendDummyClocks(uint8_t n)
{
	for (uint8_t i=0; i<n; i++) {
		SPI_Transfer(0xFF);
	}
}

// Envía un comando SD (CMDx) en modo SPI
static uint8_t SD_SendCommand(uint8_t cmd, uint32_t arg, uint8_t crc)
{
	uint8_t response;
	uint8_t retry = 0xFF;

	SPI_SD_Unselect();
	SPI_Transfer(0xFF);
	SPI_SD_Select();
	SPI_Transfer(0xFF);

	SPI_Transfer(0x40 | cmd);     // CMDx
	SPI_Transfer((uint8_t)(arg >> 24));
	SPI_Transfer((uint8_t)(arg >> 16));
	SPI_Transfer((uint8_t)(arg >> 8));
	SPI_Transfer((uint8_t)(arg));
	SPI_Transfer(crc | 0x01);

	// Esperar respuesta (bit7=0)
	do {
		response = SPI_Transfer(0xFF);
	} while ((response & 0x80) && --retry);

	return response;
}

// Envío de comando apilado ACMD (primero CMD55, luego CMDx)
static uint8_t SD_SendACMD(uint8_t cmd, uint32_t arg, uint8_t crc)
{
	uint8_t r;
	r = SD_SendCommand(55, 0, 0x01);
	if (r > 1) return r;
	return SD_SendCommand(cmd, arg, crc);
}

uint8_t SD_Init(void)
{
	uint8_t i, r;

	SPI_SD_Unselect();
	SD_SendDummyClocks(10);   // al menos 74 ciclos

	// CMD0: reset, entrar a modo SPI
	i = 0;
	do {
		r = SD_SendCommand(0, 0, 0x95);
		i++;
	} while ((r != 0x01) && (i < 100));

	if (r != 0x01) {
		SPI_SD_Unselect();
		return SD_ERR_INIT;
	}

	// Intentar inicializar con ACMD41 (SDC)
	i = 0;
	do {
		r = SD_SendACMD(41, 0, 0x01);
		i++;
	} while ((r != 0x00) && (i < 200));

	SPI_SD_Unselect();
	SPI_Transfer(0xFF);

	if (r != 0x00) return SD_ERR_INIT;

	// Opcional: fijar tamaño de bloque a 512 con CMD16
	SPI_SD_Select();
	r = SD_SendCommand(16, 512, 0x01);
	SPI_SD_Unselect();
	SPI_Transfer(0xFF);
	if (r != 0x00) return SD_ERR_INIT;

	return SD_OK;
}

uint8_t SD_ReadBlock(uint32_t lba, uint8_t *buffer)
{
	uint8_t r;
	uint16_t i;
	uint16_t timeout;

	// Para SDSC asumimos lba*512 = dirección byte.
	uint32_t addr = lba * 512UL;

	SPI_SD_Select();
	r = SD_SendCommand(17, addr, 0x01);
	if (r != 0x00) {
		SPI_SD_Unselect();
		return SD_ERR_INIT;
	}

	// Esperar token de inicio de bloque 0xFE
	timeout = 0xFFFF;
	do {
		r = SPI_Transfer(0xFF);
	} while ((r != SD_TOKEN_START_BLOCK) && --timeout);

	if (!timeout) {
		SPI_SD_Unselect();
		return SD_ERR_TIMEOUT;
	}

	// Leer 512 bytes
	for (i = 0; i < 512; i++) {
		buffer[i] = SPI_Transfer(0xFF);
	}

	// CRC (2 bytes, ignorados)
	SPI_Transfer(0xFF);
	SPI_Transfer(0xFF);

	SPI_SD_Unselect();
	SPI_Transfer(0xFF);

	return SD_OK;
}
