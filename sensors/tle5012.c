#include "tle5012.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "crc.h"
#include <math.h>
#include "spi_half_duplex.h"

#define HIGH 1
#define LOW 0

static uint PIN_SCK;
static uint PIN_MISO;
static uint PIN_MOSI;
static spi_inst_t * SPI_PORT;

static uint dma_rx;
static uint dma_tx;

#define DMA_BUF_SIZE 8
static uint8_t dma_rx_buf[DMA_BUF_SIZE];
static uint8_t dma_tx_buf[DMA_BUF_SIZE];

#define SPI_4MHz 4000000 /* Verify that this is correct */

#ifndef M_PI
	#define M_PI 3.1415926535897932384626433832795
#endif

#define TLE5012_SPI_MODE 1

static uint8_t cmd;

static void tle5012_startDMA(spi_inst_t* spi);
static void tle5012_stopDMA(spi_inst_t* spi);

void tle5012_init(spi_inst_t * spi, uint sck, uint miso, uint mosi)
{
	SPI_PORT = spi;
	PIN_SCK = sck;
	PIN_MISO = miso;
	PIN_MOSI = mosi;

	spi_init(SPI_PORT, 1000 * 1000);
	gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
}

void tle5012_Read(uint8_t * data, uint8_t addr, uint8_t length)
{
	cmd = 0x80 | (addr & 0x0F)<<3 | (length & 0x07);
	
	SPI_HalfDuplex_Transmit(&cmd, 1, TLE5012_SPI_MODE);
	if (length > 0)
	{
		SPI_HalfDuplex_Receive(data, length+1, TLE5012_SPI_MODE);
	}

}

void tle5012_Write(uint8_t * data, uint8_t addr, uint8_t length)
{
	cmd = addr<<3 | (addr & 0x0F)<<3 | (length & 0x07);
	SPI_HalfDuplex_Transmit(&cmd, 1, TLE5012_SPI_MODE);
	if (length > 0)
	{
		SPI_HalfDuplex_Transmit(data, length, TLE5012_SPI_MODE);
	}
}

int tle5012_GetAngle(spi_inst_t * sensor, float * angle)
{
	int16_t reg_data;
	float out = 0;
	int ret = 0;
	
	// if (CheckCrc(&sensor->data[0], sensor->data[5], 0xFF, 4))
	// {
	// 	reg_data = (sensor->data[2] & 0x3F)<<8 | sensor->data[3];
	// 	if (sensor->data[2] & 0x40)  reg_data -= 16384;
				
	// 	out = reg_data * 360.0f / 32768.0f;			
	// 	*angle = out;
	// 	ret = 0;
	// }
	// else
	// {
	// 	ret = -1;
	// }
	return ret;
}

static void tle5012_startDMA(spi_inst_t *spi)
{
    // TODO: Figure out how to do this
}

static void tle5012_stopDMA(spi_inst_t* spi)
{
    // TODO: Should be the inverse of whatever I'm doing in the previous function
}