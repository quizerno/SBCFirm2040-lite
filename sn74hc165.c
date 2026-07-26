#define _74HC165_SPI_

#include "pico/stdlib.h"

#define LOW 0
#define HIGH 1

static uint __clk;
static uint __shld;
static uint __ser;

void init_sn74hc165(uint shld, uint clk, uint ser) {
    //spi_init(SPI_PORT, baudrate);
    //gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    //gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    //gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    __shld = shld;
    __clk = clk;
    __ser = ser;

    gpio_init(__shld);
    gpio_init(__clk);
    gpio_init(__ser);
    
    gpio_set_dir(__shld, GPIO_OUT);
    gpio_set_dir(__clk, GPIO_OUT);
    gpio_set_dir(__ser, GPIO_IN);

    gpio_put(__shld, LOW);
    gpio_put(__clk, LOW);
}

void read_sn74hc165(uint8_t *buf, uint8_t buf_len) {
    // uint8_t data = 0;
    // gpio_put(PIN_CS, HIGH);
    // spi_read_blocking(SPI_PORT, 0, &data, 1);
    // gpio_put(PIN_CS, LOW);
    // return data;
    gpio_put(__shld, HIGH);
    sleep_us(1);
    for(int i = 0; i < buf_len; i++) {
        buf[i] = 0;
        for(int j = 0; j < 8; j++) {
            buf[i] *= 2;
            if(gpio_get(__ser))
                buf[i] += 1;
            gpio_put(__clk, HIGH);
            sleep_us(1);            // sleep for 1000 ns (minimum 130 ns)
            gpio_put(__clk, LOW);
            sleep_us(1);            // sleep for 1000 ns (minimum 130 ns)
        }
        buf[i] = ~buf[i];   // Invert because pins are pulled down when pressed
    }
    gpio_put(__shld, LOW);
}
