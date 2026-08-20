#ifndef HID_GAMEPAD_H
#define HID_GAMEPAD_H

#include <stdint.h>
#include <stdbool.h>

// Abstracted hardware parsing layout mapping 5 distinct analog axes
typedef struct {
    int8_t  lx;       // Left Stick X (-128 to 127)
    int8_t  ly;       // Left Stick Y (-128 to 127)
    int8_t  rx;       // Right Stick X (-128 to 127)
    int8_t  ry;       // Right Stick Y (-128 to 127)
    int16_t z_trigger;// Combined Z axis representing L2/R2 triggers (-255 to 255)
    uint8_t hat;      // D-pad positioning state (0-7 for active, 8 for IDLE RELEASED)
    uint32_t buttons; // Raw uniform digital button map bits
	
	// Continuous Touchpad Stream Data
    uint8_t  tpad_packets; // Sequence transaction loop counter
    bool     finger_active;
    uint16_t finger_x;     
    uint16_t finger_y; 
	
} generic_gamepad_data_t;

// Framework Parsing Hooks
bool parse_ps4_controller(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_generic_hid_gamepad(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);

#endif // HID_GAMEPAD_H
