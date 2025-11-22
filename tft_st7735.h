// tft_st7735.h
#ifndef TFT_ST7735_H_
#define TFT_ST7735_H_
#define TFT_WIDTH 132
#define TFT_HEIGHT 162

#include <stdint.h>

void TFT_Init(void);
void TFT_FillScreen(uint16_t color);

void TFT_SetAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void TFT_StartWrite(void);
void TFT_WriteColor(uint16_t color);
void TFT_EndWrite(void);

#endif /* TFT_ST7735_H_ */
