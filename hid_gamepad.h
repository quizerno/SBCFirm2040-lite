#ifndef HID_GAMEPAD_H
#define HID_GAMEPAD_H

#include <stdint.h>
#include <stdbool.h>

// Standard layout structure for a Native Steel Battalion Controller report packet
typedef struct TU_ATTR_PACKED {
    uint8_t  report_id;          // Typically 0x01
    uint32_t buttons_block1;     // Digital Button Matrix Bank 1 (Face buttons, toggles)
    uint32_t buttons_block2;     // Digital Button Matrix Bank 2 (Eject, Ignition, Start)
    int16_t  rotation_lever;     // Main steer steering/rotation mechanism
    int16_t  sight_change_x;     // Aiming camera horizontal coordinate channel
    int16_t  sight_change_y;     // Aiming camera vertical coordinate channel
    int16_t  aiming_x;           // Main weapon trajectory articulation channel X
    int16_t  aiming_y;           // Main weapon trajectory articulation channel Y
    uint8_t  gear_lever;         // Physical transmission block location (R, N, 1, 2, 3, 4, 5)
    uint8_t  clutch_pedal;       // Left foot step axis pressure values
    uint8_t  brake_pedal;        // Center foot step axis pressure values
    uint8_t  accelerator_pedal;  // Right foot step axis pressure values
} steel_battalion_native_report_t;

// Keep your existing structures below...
typedef struct {
    int8_t  lx;
    int8_t  ly;
    int8_t  rx;
    int8_t  ry;
    int16_t z_trigger;
    uint8_t hat;
    uint32_t buttons;
    uint8_t  tpad_packets;
    bool     finger_active;
    uint16_t finger_x;     
    uint16_t finger_y; 
} generic_gamepad_data_t;


bool parse_ps4_controller(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_generic_hid_gamepad(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_logitech_dual_action(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_steel_battalion_native(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_generic_joystick_interface(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
#endif // HID_GAMEPAD_H
