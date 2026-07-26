#include "as5600.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <machine/endian.h>

/* We use i2c0 */
#define I2C i2c0

/* -- Operation Speeds -- */
#define BAUDRATE_NORMAL (100 * 1000)   /* 100 kHz */
#define BAUDRATE_FASTMODE (400 * 1000) /* 400 kHz */
#define BAUDRATE_FASTMODEPLUS (1000 * 1000) /* 1 MHz */
#define BAUDRATE_DEFAULT BAUDRATE_NORMAL
/* -- Operaton Speeds */

/* -- AS5600 Slave Address -- */
#define AS5600_ADDR 0x36
/* -- AS5600 Slave Address -- */

#define I2C_TIMEOUT_US (100000)
#define PIN_DIRECTION 21

/* -- AS5600 Registers -- */
#define REG_ZMCO 0x00
#define REG_ZPOS_MSB 0x01
#define REG_ZPOS_LSB 0x02
#define REG_MPOS_MSB 0x03
#define REG_MPOS_LSB 0x04
#define REG_MANG_MSB 0x05
#define REG_MANGA_LSB 0x06
#define REG_CONF_MSB 0x07
#define REG_CONF_LSB 0x08
#define REG_RAWANGLE_MSB 0x0C
#define REG_RAWANGLE_LSB 0x0D
#define REG_ANGLE_MSB 0x0E
#define REG_ANGLE_LSB 0x0F
#define REG_STATUS 0x0B
#define REG_AGC 0x1A
#define REG_MAGNITUDE_MSB 0x1B
#define REG_MAGNITUDE_LSB 0x1C
#define REG_BURN 0xFF
/* -- AS5600 Registers -- */

#define PIN_SDA 8
#define PIN_SCL 9

extern void i2c_error(int error);

void as5600_init()
{
    i2c_init(I2C, BAUDRATE_DEFAULT);             // default to 100kHz
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);
    
    // This is where we can programmatically set the direction
    gpio_init(PIN_DIRECTION);
    gpio_set_dir(PIN_DIRECTION, GPIO_OUT);
    gpio_put(PIN_DIRECTION, 0);
    sleep_ms(1000);
}

static int as5600_read_reg(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    int result = i2c_write_timeout_us(I2C, AS5600_ADDR, &reg, 1, true, I2C_TIMEOUT_US); // Write Register
    if(result <= 0)
        return result;
    return i2c_read_timeout_us(I2C, AS5600_ADDR, buffer, length, false, I2C_TIMEOUT_US);         // Read Value
}

uint8_t as5600_getStatus() {
    uint8_t result = 0;
    int response = as5600_read_reg(REG_STATUS, &result, 1);
    if(response != 0)
        i2c_error(response);
    return result & 0x38;
}

float as5600_getRawAngle()
{
    int ret = 0;
    uint16_t raw_angle = 0;
    
    ret = as5600_read_reg(REG_RAWANGLE_MSB, (uint8_t*)&raw_angle, sizeof(raw_angle));
    if (ret == 0) {
        raw_angle = __bswap16(raw_angle) & 0xFFF;
        return raw_angle / 4192.0f;
    }
    i2c_error(ret);
    return -1.0f;
}

float as5600_getAngle()
{
    int ret = 0;
    uint16_t angle = 0;
    
    ret = as5600_read_reg(REG_ANGLE_MSB, (uint8_t*)&angle, sizeof(angle));
    if (ret == 0) {
        angle = __bswap16(angle) & 0xFFF;
        return angle / 4192.0f;
    }
    i2c_error(ret);
    return -1.0f;
}