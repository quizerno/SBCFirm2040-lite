#ifndef __AS5600_H__
#define __AS5600_H__

#include <inttypes.h>

void as5600_init();

uint8_t as5600_getStatus();

float as5600_getRawAngle();

float as5600_getAngle();

#endif // __AS5600_H__