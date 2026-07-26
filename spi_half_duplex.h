#ifndef __HALF_DUPLEX_SPI_H__
#define __HALF_DUPLEX_SPI_H__

void SPI_HalfDuplex_Transmit(uint8_t *cmd, uint16_t length, uint8_t spi_mode);

void SPI_HalfDuplex_Receive(uint8_t *cmd, uint16_t length, uint8_t spi_mode);

#endif // __HALF_DUPLEX_SPI_H__