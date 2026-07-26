#ifndef __CRC_H__
#define __CRC_H__

#include <inttypes.h>

uint8_t MathCRC8(uint8_t crc, uint8_t data);

uint8_t CheckCrc(uint8_t * data, uint8_t crc, uint8_t initial, uint8_t length);

#endif // __CRC_H__