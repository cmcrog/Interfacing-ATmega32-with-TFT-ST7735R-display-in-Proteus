// spi_hal.h
#ifndef SPI_HAL_H_
#define SPI_HAL_H_

#include <avr/io.h>
#include <stdint.h>

// Pines SPI del ATmega32
// MOSI = PB5, MISO = PB6, SCK = PB7 (hardware SPI)
#define SPI_DDR      DDRB
#define SPI_PORT     PORTB
#define SPI_PIN_REG  PINB

#define SPI_MOSI     PB5
#define SPI_MISO     PB6
#define SPI_SCK      PB7

// Chip Select del TFT (antes era tu SS_PIN)
#define TFT_CS_DDR   DDRB
#define TFT_CS_PORT  PORTB
#define TFT_CS_PIN   PB4

// Chip Select de la SD (ejemplo: PD2)
#define SD_CS_DDR    DDRD
#define SD_CS_PORT   PORTD
#define SD_CS_PIN    PD2

// Pines de control del TFT
#define TFT_DC_DDR   DDRB
#define TFT_DC_PORT  PORTB
#define TFT_DC_PIN   PB1

#define TFT_RST_DDR  DDRB
#define TFT_RST_PORT PORTB
#define TFT_RST_PIN  PB0

void SPI_Init(uint8_t clock_div);      // clock_div: 2,4,8,16,32,64,128 (aprox)
uint8_t SPI_Transfer(uint8_t data);

void SPI_TFT_Select(void);
void SPI_TFT_Unselect(void);

void SPI_SD_Select(void);
void SPI_SD_Unselect(void);

void TFT_DC_Command(void);
void TFT_DC_Data(void);

void TFT_Reset_Pulse(void);

#endif /* SPI_HAL_H_ */
