#ifndef HID_GAMEPAD_H
#define HID_GAMEPAD_H

#include <stdint.h>
#include <stdbool.h>

// Abstracted hardware parsing layout 
typedef struct {
    int8_t  x, y, z, rz; // Pure hardware analog axes (-128 to 127)
    uint8_t hat;         // D-pad positioning state
    uint32_t buttons;    // Raw unmapped button bits
} generic_gamepad_data_t;

// Framework Parsing Hooks
bool parse_ps4_controller(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_generic_hid_gamepad(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);

#endif // HID_GAMEPAD_H
