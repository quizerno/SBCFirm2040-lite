#include "tle5011.h"
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

#define TLE5011_SPI_MODE 1

static uint8_t cmd;

void tle5011_init(spi_inst_t * spi, uint sck, uint miso, uint mosi) {
    SPI_PORT = spi;
    PIN_SCK = sck;
    PIN_MISO = miso;
    PIN_MOSI = mosi;

    spi_init(SPI_PORT, SPI_4MHz);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    uint dma_tx = dma_claim_unused_channel(true);
    uint dma_rx = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, true));
    dma_channel_configure(dma_tx, &c,
                          &spi_get_hw(SPI_PORT)->dr, // write address
                          dma_tx_buf, // read address
                          DMA_BUF_SIZE, // element count (each element is of size transfer_data_size)
                          false); // don't start yet

    c = dma_channel_get_default_config(dma_rx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, false));
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure(dma_rx, &c,
                          dma_rx_buf, // write address
                          &spi_get_hw(SPI_PORT)->dr, // read address
                          DMA_BUF_SIZE, // element count (each element is of size transfer_data_size)
                          false); // don't start yet
}

void tle5011_init_sensor(uint cs) {
    gpio_init(cs);
    gpio_set_dir(cs, GPIO_OUT);
    gpio_put(cs, HIGH);
}

static void tle5011_Read(uint8_t * data, uint8_t addr, uint8_t length)
{
	cmd = 0x80 | (addr & 0x0F)<<3 | (length & 0x07);
	
	SPI_HalfDuplex_Transmit(&cmd, 1, TLE5011_SPI_MODE);
	if (length > 0)
	{
		SPI_HalfDuplex_Receive(data, length+1, TLE5011_SPI_MODE);
	}

}

static void tle5011_Write(uint8_t * data, uint8_t addr, uint8_t length)
{
	cmd = addr<<3 | (addr & 0x0F)<<3 | (length & 0x07);
	SPI_HalfDuplex_Transmit(&cmd, 1, TLE5011_SPI_MODE);
	if (length > 0)
	{
		SPI_HalfDuplex_Transmit(data, length, TLE5011_SPI_MODE);
	}
}

int tle5011_getAngle(uint cs, float *angle) {
    int16_t x_value, y_value;
	float out = 0;
	int ret = -1;

    spi_hw_t* spi_hw = spi_get_hw(SPI_PORT);
	
	// if (CheckCrc(&spi_hw->data[2], spi_hw->data[6], 0xFB, 4))
	// {
	// 	x_value = spi_hw->data[3]<<8 | spi_hw->data[2];
	// 	y_value = spi_hw->data[5]<<8 | spi_hw->data[4];
		
	// 	if((x_value != 32767) && (x_value != -32768) && (x_value != 0) &&
	// 		 (y_value != 32767) && (y_value != -32768) && (y_value != 0))
	// 	{				
	// 		out = atan2f((float)y_value, (float)x_value)/ M_PI * (float)180.0;			
	// 		*angle = out;
	// 		ret = 0;
	// 	}
	// }
	return ret;
}

static void tle5011_startDMA(spi_inst_t *spi)
{
    // TODO: Figure out how to do this
}

static void tle5011_stopDMA(spi_inst_t* spi)
{
    // TODO: Should be the inverse of whatever I'm doing in the previous function
}