#include "hid_gamepad.h"
#include <stdlib.h>
#include <string.h>

typedef struct TU_ATTR_PACKED {
    uint8_t x, y, z, rz;
    struct {
        uint8_t dpad     : 4; 
        uint8_t square   : 1; 
        uint8_t cross    : 1; 
        uint8_t circle   : 1; 
        uint8_t triangle : 1; 
    };
    struct {
        uint8_t l1     : 1;
        uint8_t r1     : 1;
        uint8_t l2     : 1;
        uint8_t r2     : 1;
        uint8_t share  : 1;
        uint8_t option : 1;
        uint8_t l3     : 1;
        uint8_t r3     : 1;
    };
    struct {
        uint8_t ps      : 1; 
        uint8_t tpad    : 1; 
        uint8_t counter : 6; 
    };
    uint8_t l2_trigger; 
    uint8_t r2_trigger; 
} sony_ds4_report_t;

bool parse_ps4_controller(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    if (report[0] != 1 || len < sizeof(sony_ds4_report_t) + 1) return false;
    
    sony_ds4_report_t ds4;
    memcpy(&ds4, &report[1], sizeof(sony_ds4_report_t));

    out_data->x  = (int16_t)ds4.x - 128;
    out_data->y  = (int16_t)ds4.y - 128;
    out_data->z  = (int16_t)ds4.z - 128;
    out_data->rz = (int16_t)ds4.rz - 128;
    out_data->hat = (ds4.dpad < 9) ? ds4.dpad : 0;

    out_data->buttons = 0;
    out_data->buttons |= (ds4.cross    << 0); // Map Cross to Bit 0
    out_data->buttons |= (ds4.circle   << 1); // Map Circle to Bit 1
    out_data->buttons |= (ds4.square   << 2); // Map Square to Bit 2
    out_data->buttons |= (ds4.triangle << 3); // Map Triangle to Bit 3
    out_data->buttons |= (ds4.l1       << 4);
    out_data->buttons |= (ds4.r1       << 5);
    out_data->buttons |= (ds4.share    << 6);
    out_data->buttons |= (ds4.option   << 7);
    out_data->buttons |= (ds4.ps       << 8);
    return true;
}

bool parse_generic_hid_gamepad(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    if (len < 3) return false;

    out_data->x  = (int16_t)report[0] - 128;
    out_data->y  = (int16_t)report[1] - 128;
    out_data->z  = (len >= 4) ? ((int16_t)report[2] - 128) : 0;
    out_data->rz = (len >= 4) ? ((int16_t)report[3] - 128) : 0;
    out_data->hat = (len >= 5) ? (report[4] & 0x0F) : 0;
    if (out_data->hat >= 9) out_data->hat = 0;

    out_data->buttons = 0;
    if (len >= 6) out_data->buttons |= ((uint32_t)report[5]);
    if (len >= 7) out_data->buttons |= ((uint32_t)report[6] << 8);
    if (len >= 8) out_data->buttons |= ((uint32_t)report[7] << 16);
    return true;
}
