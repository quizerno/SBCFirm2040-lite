#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "tusb.h"
#include "bsp/board_api.h"
#include "tusb_gamepad.h"

#include "led.h"
#include "sn74hc165.h"
#include "sensors/tle5011.h"
#include "sensors/tle5012.h"
#include "sensors/as5600.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"

#define _74HC165_SHLD 26
#define _74HC165_CLK 22
#define _74HC165_SER 20

#define AS5600_SDA 8
#define AS5600_SCL 9

#define MOUNTED_R 0x00
#define MOUNTED_G 0x90
#define MOUNTED_B 0x16

#define UNMOUNTED_R 0x90
#define UNMOUNTED_G 0x16
#define UNMOUNTED_B 0x00

#define AXIS_DIR_PIN 10
#define AXIS_0_PIN 18
#define AXIS_1_PIN 19

#define MIN_X 27000
#define MAX_X 35000

#define MAX_Y 33000
#define MIN_Y 25000

static uint8_t led_color_r = 255;
static uint8_t led_color_g = 0;
static uint8_t led_color_b = 0;

// Limit max brightness (at compile-time)
#define FULL_BRIGHT 0x20
#define Q0 0
#define Q1 (uint8_t)(FULL_BRIGHT / 4)
#define Q2 (uint8_t)(FULL_BRIGHT / 2)
#define Q3 (uint8_t)((3 * FULL_BRIGHT) / 4)
#define Q4 FULL_BRIGHT

// RGB Pattern
uint8_t led_colors_r[] = { Q4, Q4, Q4, Q4, Q4, Q3, Q2, Q1, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q1, Q2, Q3, Q4, Q4, Q4, Q4 };
uint8_t led_colors_g[] = { Q0, Q1, Q2, Q3, Q4, Q4, Q4, Q4, Q4, Q4, Q4, Q4, Q4, Q3, Q2, Q1, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0 };
uint8_t led_colors_b[] = { Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q1, Q2, Q3, Q4, Q4, Q4, Q4, Q4, Q4, Q4, Q4, Q4, Q3, Q2, Q1 };

void led_test(void);
void led_task(void);
void i2c_error(int error);

static float axis_0;
static float axis_1;

static inline void change_led_color(uint8_t r, uint8_t g, uint8_t b)
{
    led_color_r = r;
    led_color_g = g;
    led_color_b = b;
}

void update_gamepad(Gamepad *gp);

int main(void)
{
    axis_0 = 0.0f;
    axis_1 = 0.0f;

    // set_sys_clock_khz(120000, true);
    board_init();

    init_tusb_gamepad(INPUT_MODE_XBOXORIGINAL);

    stdio_init_all();

    gpio_init(AXIS_DIR_PIN);
    gpio_set_dir(AXIS_DIR_PIN, GPIO_OUT);
    gpio_pull_down(AXIS_DIR_PIN);

    adc_init();
    adc_gpio_init(AXIS_0_PIN);
    adc_gpio_init(AXIS_1_PIN);

    led_init();

    //init_sn74hc165(_74HC165_SHLD, _74HC165_CLK, _74HC165_SER);
    //as5600_init();
    //tle5012_init(spi0, _74HC165_CLK, _74HC165_SER, _74HC165_SHLD);

    Gamepad *gp = gamepad(0);

    while (1)
    {
        update_gamepad(gp);

        tusb_gamepad_task();

        sleep_ms(1);
        tud_task();
        led_task();
    }

    return 0;
}

const float normalize = (1.0f / 0xFFF);

void update_gamepad(Gamepad *gp)
{
    //uint8_t data[] = {0};
    //read_sn74hc165(data, sizeof(data));
    //memcpy(&gp->steel_battalion_in_report.dButtons, data, sizeof(data));
    //uint8_t status = as5600_getStatus();
    //if(status != 0)
    //    i2c_error(status);
    adc_select_input(0);
    uint32_t rawValue = adc_read();
    int32_t adjusted = rawValue * 65535 / 4095;
//    adjusted = (adjusted - MIN_X) * (65535 / (MAX_X - MIN_X));
    if(adjusted < 0)
        adjusted = 0;
    else if(adjusted > 65535)
        adjusted = 65535;

    axis_0 = adjusted / 65535.0f;

    gp->steel_battalion_in_report.rotationLever = (int16_t)(adjusted - 32768);
}

// float pulse_brightness = 0;
// float d_pulse_brightness = 1.0f / 256.0f;
static int color_index = 0;
void led_task(void)
{
    float b = axis_0;
    float g = axis_1;
    float r = 1.0f - (axis_0 * axis_0) - (axis_1 * axis_1);
    set_led((uint8_t)(255 * r), (uint8_t)(255 * g), (uint8_t)(255 * b));
    //set_led(led_colors_r[color_index], led_colors_g[color_index], led_colors_b[color_index]);
    //color_index = (color_index + 1) % sizeof(led_colors_r);
}

void led_test(void)
{
    const uint interval = 30;

    for (int i = 0; i < sizeof(led_color_r); i++)
    {
        set_led(led_colors_r[i], led_colors_g[i], led_colors_b[i]);
        sleep_ms(interval);
    }
}

void error_led(uint8_t r, uint8_t g, uint8_t b, int ms_on, int ms_off)
{
    //while(1) {
        set_led(r, g, b);
        sleep_ms(ms_on);
        set_led(0, 0, 0);
        sleep_ms(ms_off);
    //}
}

void i2c_error(int error) {
    if(error == PICO_ERROR_GENERIC)
        error_led(150, 0, 0, 500, 500);
    if(error == PICO_ERROR_TIMEOUT)
        error_led(150, 0, 0, 30, 500);
    else if(error == 0x10)
        error_led(0, 0, 150, 700, 300);
    else if(error == 0x20)
        error_led(0, 0, 150, 300, 700);
    else if(error == 0x08)
        error_led(0, 0, 150, 250, 250);
    else
        error_led(150, 0, 0, 30, 60);
}
