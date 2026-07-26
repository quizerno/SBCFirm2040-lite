#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "led.h"

#define __PIO pio1
#define __sm 1

#define IS_RGBW true
#define NUM_PIXELS 1

#ifdef PICO_DEFAULT_WS2812_PIN
#define WS2812_PIN PICO_DEFAULT_WS2812_PIN
#else
#warning PICO_DEFAULT_WS2812_PIN not defined, falling back to 25
#define WS2812_PIN 25
#endif

static inline void put_pixel(uint32_t pixel_rgb) {
    pio_sm_put_blocking(__PIO, __sm, pixel_rgb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8)  |
           ((uint32_t)(g) << 16) |
            (uint32_t)(b);
}

static bool led_already_init = false;

void led_init() {
    if(led_already_init)
        return;

	uint offset = pio_add_program(__PIO, &ws2812_program);

    ws2812_program_init(__PIO, __sm, offset, WS2812_PIN, 800000, IS_RGBW);

    led_already_init = true;
}

void set_led(uint8_t r, uint8_t g, uint8_t b) {
	put_pixel(urgb_u32(r, g, b));
}
