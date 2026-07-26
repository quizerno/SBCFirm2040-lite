#include "spi_half_duplex.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

void SPI_HalfDuplex_Transmit(uint8_t *cmd, uint16_t length, uint8_t spi_mode) {
    // TODO: Implement this
    // DMA_InitTypeDef DMA_InitStructure;
		
	// DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t) data;
	// DMA_InitStructure.DMA_BufferSize = length;
	// DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t) &SPI1->DR;
	// DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
	// DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	// DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	// DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	// DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	// DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	// DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	// DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
	// DMA_Init(DMA1_Channel3, &DMA_InitStructure);
	
	// DMA_ITConfig(DMA1_Channel3, DMA_IT_TC, ENABLE);
	// NVIC_SetPriority(DMA1_Channel3_IRQn, 2);
	// NVIC_EnableIRQ(DMA1_Channel3_IRQn);
	
	// // Set haft-duplex tx
	// uint16_t cr1temp = SPI1->CR1;
	// cr1temp |= SPI_CR1_SPE;	
	// cr1temp &= ~(SPI_CR1_CPOL|SPI_CR1_CPHA);
	// cr1temp |= SPI_CR1_BIDIMODE | (spi_mode & 0x03);
	// SPI1->CR1 = cr1temp;
	// SPI1->DR;							// clear RXNE 
	// SPI_BiDirectionalLineConfig(SPI1, SPI_Direction_Tx);
	
	// SPI_I2S_ReceiveData(SPI1);
	// SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_OVR);

	// DMA_Cmd(DMA1_Channel3, ENABLE);
}

void SPI_HalfDuplex_Receive(uint8_t *cmd, uint16_t length, uint8_t spi_mode) {
    // TODO: Implement this
    // DMA_InitTypeDef DMA_InitStructure;
		
	// DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t) data;
	// DMA_InitStructure.DMA_BufferSize = length;
	// DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t) &SPI1->DR;
	// DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	// DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	// DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	// DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	// DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	// DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	// DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	// DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
	// DMA_Init(DMA1_Channel2, &DMA_InitStructure);
	
	// DMA_ITConfig(DMA1_Channel2, DMA_IT_TC, ENABLE);
	// NVIC_SetPriority(DMA1_Channel2_IRQn, 2);
	// NVIC_EnableIRQ(DMA1_Channel2_IRQn);
	
	// // Set half-duplex rx
	// uint16_t cr1temp = SPI1->CR1;
	// cr1temp |= SPI_CR1_SPE;	
	// cr1temp &= ~(SPI_CR1_CPOL|SPI_CR1_CPHA);
	// cr1temp |= SPI_CR1_BIDIMODE | (spi_mode & 0x03);
	// SPI1->CR1 = cr1temp;
	// SPI_BiDirectionalLineConfig(SPI1, SPI_Direction_Rx);
	
	// SPI_I2S_ReceiveData(SPI1);
	// SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_OVR);

	// DMA_Cmd(DMA1_Channel2, ENABLE);
}