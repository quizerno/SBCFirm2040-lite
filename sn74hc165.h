#ifndef __SN74HC165_H__
#define __SN74HC165_H__

#include <inttypes.h>

void init_sn74hc165(uint shld, uint clk, uint ser);

void read_sn74hc165(uint8_t *buf, uint8_t buf_len);

#endif