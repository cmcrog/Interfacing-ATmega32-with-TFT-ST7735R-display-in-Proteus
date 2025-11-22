// tft_st7735.c
#include "tft_st7735.h"
#include "spi_hal.h"

// Comandos ST7735
#define ST7735_SWRESET  0x01
#define ST7735_SLPOUT   0x11
#define ST7735_COLMOD   0x3A
#define ST7735_DISPON   0x29
#define ST7735_CASET    0x2A
#define ST7735_RASET    0x2B
#define ST7735_RAMWR    0x2C
#define ST7735_MADCTL   0x36

static void TFT_WriteCommand(uint8_t cmd)
{
	TFT_DC_Command();
	SPI_TFT_Select();
	SPI_Transfer(cmd);
	SPI_TFT_Unselect();
}

static void TFT_WriteData(uint8_t data)
{
	TFT_DC_Data();
	SPI_TFT_Select();
	SPI_Transfer(data);
	SPI_TFT_Unselect();
}

static void TFT_WriteData16(uint16_t data)
{
	TFT_DC_Data();
	SPI_TFT_Select();
	SPI_Transfer(data >> 8);
	SPI_Transfer(data & 0xFF);
	SPI_TFT_Unselect();
}

void TFT_Init(void)
{
	// Reset físico
	TFT_Reset_Pulse();

	// Inicio de secuencia (simplificada)
	TFT_WriteCommand(ST7735_SWRESET);
	for (volatile uint32_t i=0; i<80000; i++); // delay ~

	TFT_WriteCommand(ST7735_SLPOUT);
	for (volatile uint32_t i=0; i<80000; i++);

	// Modo 16 bits por píxel
	TFT_WriteCommand(ST7735_COLMOD);
	TFT_WriteData(0x05); // 16-bit color

	// Dirección (MADCTL) básica
	TFT_WriteCommand(ST7735_MADCTL);
	TFT_WriteData(0x00); // ajustar luego según rotación

	// Encender display
	TFT_WriteCommand(ST7735_DISPON);
	for (volatile uint32_t i=0; i<80000; i++);
}

// Define región de escritura (x0..x1, y0..y1)
void TFT_SetAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
	TFT_WriteCommand(ST7735_CASET);
	TFT_DC_Data();
	SPI_TFT_Select();
	SPI_Transfer(0x00);
	SPI_Transfer(x0);
	SPI_Transfer(0x00);
	SPI_Transfer(x1);
	SPI_TFT_Unselect();

	TFT_WriteCommand(ST7735_RASET);
	TFT_DC_Data();
	SPI_TFT_Select();
	SPI_Transfer(0x00);
	SPI_Transfer(y0);
	SPI_Transfer(0x00);
	SPI_Transfer(y1);
	SPI_TFT_Unselect();

	TFT_WriteCommand(ST7735_RAMWR);
}

// Para streaming continuo
void TFT_StartWrite(void)
{
	TFT_DC_Data();
	SPI_TFT_Select();
}

void TFT_WriteColor(uint16_t color)
{
	SPI_Transfer(color >> 8);
	SPI_Transfer(color & 0xFF);
}

void TFT_EndWrite(void)
{
	SPI_TFT_Unselect();
}

// Llenar toda la pantalla (ejemplo para 128x160)
void TFT_FillScreen(uint16_t color)
{
	uint16_t x, y;
	TFT_SetAddrWindow(0,0,TFT_WIDTH - 1,TFT_HEIGHT - 1);
	TFT_StartWrite();
	for (y = 0; y < TFT_HEIGHT; y++) {
		for (x = 0; x < TFT_WIDTH; x++) {
			TFT_WriteColor(color);
		}
	}
	TFT_EndWrite();
}
