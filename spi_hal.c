// spi_hal.c
#include "spi_hal.h"

// Inicializa SPI como maestro
void SPI_Init(uint8_t clock_div)
{
	// MOSI, SCK y TFT_CS, SD_CS, DC, RST como salida
	SPI_DDR |= (1<<SPI_MOSI) | (1<<SPI_SCK);
	TFT_CS_DDR |= (1<<TFT_CS_PIN);
	SD_CS_DDR  |= (1<<SD_CS_PIN);
	TFT_DC_DDR |= (1<<TFT_DC_PIN);
	TFT_RST_DDR |= (1<<TFT_RST_PIN);

	// Des-seleccionar ambos esclavos
	TFT_CS_PORT |= (1<<TFT_CS_PIN);
	SD_CS_PORT  |= (1<<SD_CS_PIN);

	// MISO como entrada
	SPI_DDR &= ~(1<<SPI_MISO);

	// Configuración base: Maestro, habilitado
	uint8_t spr = 0;
	uint8_t spi2x = 0;

	// clock_div ~ F_CPU / [div]
	switch(clock_div) {
		case 2:  spr = 0; spi2x = 1; break;
		case 4:  spr = 0; spi2x = 0; break;
		case 8:  spr = 1; spi2x = 1; break;
		case 16: spr = 1; spi2x = 0; break;
		case 32: spr = 2; spi2x = 1; break;
		case 64: spr = 2; spi2x = 0; break;
		case 128:
		default: spr = 3; spi2x = 0; break;
	}

	SPCR = (1<<SPE) | (1<<MSTR) | (spr & 0x03);
	if (spi2x) SPSR |= (1<<SPI2X);
	else       SPSR &= ~(1<<SPI2X);
}

// Envía y recibe un byte por SPI
uint8_t SPI_Transfer(uint8_t data)
{
	SPDR = data;
	while(!(SPSR & (1<<SPIF)));
	return SPDR;
}

// Seleccionar / deseleccionar TFT
void SPI_TFT_Select(void)
{
	TFT_CS_PORT &= ~(1<<TFT_CS_PIN);
}
void SPI_TFT_Unselect(void)
{
	TFT_CS_PORT |= (1<<TFT_CS_PIN);
}

// Seleccionar / deseleccionar SD
void SPI_SD_Select(void)
{
	SD_CS_PORT &= ~(1<<SD_CS_PIN);
}
void SPI_SD_Unselect(void)
{
	SD_CS_PORT |= (1<<SD_CS_PIN);
}

// DC: comando o dato
void TFT_DC_Command(void)
{
	TFT_DC_PORT &= ~(1<<TFT_DC_PIN); // DC=0
}
void TFT_DC_Data(void)
{
	TFT_DC_PORT |= (1<<TFT_DC_PIN);  // DC=1
}

// Reset corto del TFT
void TFT_Reset_Pulse(void)
{
	TFT_RST_PORT &= ~(1<<TFT_RST_PIN);
	for (volatile uint32_t i=0; i<8000; i++); // delay simple
	TFT_RST_PORT |= (1<<TFT_RST_PIN);
	for (volatile uint32_t i=0; i<8000; i++);
}
