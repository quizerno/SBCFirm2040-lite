#ifndef __TLE5011_H__
#define __TLE5011_H__

#include "hardware/spi.h"

void tle5011_init(spi_inst_t * spi, uint sck, uint miso, uint mosi);

void tle5011_init_sensor(uint cs);

int tle5011_getAngle(uint cs, float *out);

#endif // __TLE5011_H__