#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <stdint.h>

#define NEOPIXEL_PIN 21

// Initializes the PIO state machine for the NeoPixel strip
void neopixel_init(void);

// Sets the color of our single NeoPixel (RGB format)
void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b);

#endif // NEOPIXEL_H
