#include "neopixel.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

static PIO pio_instance = pio1; 
static uint sm_instance = 0;

static const uint16_t ws2812_instructions[] = {
    0x6221, //  0: out    x, 1           side 0  
    0x1123, //  1: jmp    !x, 3          side 1  
    0x1400, //  2: jmp    0              side 1  
    0xa442, //  3: nop                   side 0  
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_instructions,
    .length = 4,
    .origin = -1,
};

/* void neopixel_init(void) {
    sm_instance = pio_claim_unused_sm(pio_instance, true);
    uint offset = pio_add_program(pio_instance, &ws2812_program);

    pio_gpio_init(pio_instance, NEOPIXEL_PIN);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_instance, NEOPIXEL_PIN, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 3);
    sm_config_set_out_pins(&c, NEOPIXEL_PIN, 1);
    sm_config_set_sideset_pins(&c, NEOPIXEL_PIN);

    sm_config_set_out_shift(&c, false, true, 24); 
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    float div = clock_get_hz(clk_sys) / (800000.0f * 10.0f);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio_instance, sm_instance, offset, &c);
    pio_sm_set_enabled(pio_instance, sm_instance, true);
} */

void neopixel_init(void) {
    sm_instance = pio_claim_unused_sm(pio_instance, true);
    uint offset = pio_add_program(pio_instance, &ws2812_program);

    pio_gpio_init(pio_instance, NEOPIXEL_PIN);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_instance, NEOPIXEL_PIN, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 3);
    sm_config_set_out_pins(&c, NEOPIXEL_PIN, 1);
    
    // =========================================================================
    // FIX: EXPLICITLY DEFINE THE 1-BIT SIDE-SET PROPERTY FOR THE SM ENGINE
    // =========================================================================
    sm_config_set_sideset(&c, 1, false, false); // Define 1 side-set pin, not optional, not pindir
    sm_config_set_sideset_pins(&c, NEOPIXEL_PIN);

    sm_config_set_out_shift(&c, false, true, 24); 
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Scale clock speed to exactly 800kHz data rates (10 cycles per bit)
    float div = clock_get_hz(clk_sys) / (800000.0f * 10.0f);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio_instance, sm_instance, offset, &c);
    pio_sm_set_enabled(pio_instance, sm_instance, true);
}




void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b) {
    // WS2812B GRB struct conversion
    uint32_t val = ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | (uint32_t)(b);
    pio_sm_put_blocking(pio_instance, sm_instance, val << 8);
}















/* #include "neopixel.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

static PIO pio_instance = pio1; // Use PIO1 since PIO0 is likely heavily busy with pio_usb
static uint sm_instance = 0;

// Inline assembly for WS2812B (NeoPixel) protocol at 800kHz
static const uint16_t ws2812_instructions[] = {
    0x6221, //  0: out    x, 1           side 0 [2]  
    0x1123, //  1: jmp    !x, 3          side 1 [1]  
    0x1400, //  2: jmp    0              side 1 [4]  
    0xa442, //  3: nop                   side 0 [4]  
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_instructions,
    .length = 4,
    .origin = -1,
};

void neopixel_init(void) {
    // 1. Claim a free state machine on PIO1
    sm_instance = pio_claim_unused_sm(pio_instance, true);
    uint offset = pio_add_program(pio_instance, &ws2812_program);

    // 2. Configure the PIO GPIO settings
    pio_gpio_init(pio_instance, NEOPIXEL_PIN);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_instance, NEOPIXEL_PIN, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 3);
    sm_config_set_out_pins(&c, NEOPIXEL_PIN, 1);
    
    // Set side-set pin for visual high/low timing transitions
    sm_config_set_sideset_pins(&c, NEOPIXEL_PIN);

    // 3. Configure shift registers (MSB first, auto-pull enabled at 24 bits)
    sm_config_set_out_shift(&c, false, true, 24); 
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // 4. Scale clock speed to exactly 800kHz data rates
    float div = clock_get_hz(clk_sys) / (800000.0f * 10.0f);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio_instance, sm_instance, offset, &c);
    pio_sm_set_enabled(pio_instance, sm_instance, true);
}

void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b) {
    // NeoPixels accept data in GRB (Green, Red, Blue) byte order structure
    uint32_t val = ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | (uint32_t)(b);
    pio_sm_put_blocking(pio_instance, sm_instance, val << 8);
}





 */