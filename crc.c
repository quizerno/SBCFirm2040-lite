#include "crc.h"

uint8_t MathCRC8(uint8_t crc, uint8_t data)
{
    crc ^= data;
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        if ((crc & 0x80) != 0)
        {
            crc <<= 1;
            crc ^= 0x1D;
        }
        else
        {
            crc <<= 1;
        };
    };
    return (crc);
}

uint8_t CheckCrc(uint8_t *data, uint8_t crc, uint8_t initial, uint8_t length)
{
    uint8_t ret = initial;
    uint8_t index = 0;

    while (index < length)
    {
        ret = MathCRC8(ret, data[index++]);
    }
    ret = ~ret;

    return (ret == crc);
}