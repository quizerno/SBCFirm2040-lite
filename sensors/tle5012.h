#ifndef __TLE5012_H__
#define __TLE5012_H__

#include "hardware/spi.h"

void tle5012_init(spi_inst_t * spi, uint sck, uint miso, uint mosi);

void tle5012_init_sensor(uint cs);

int tle5012_getAngle(uint cs, float *out);

#endif // __TLE5012_H__