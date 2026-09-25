#ifndef HID_GAMEPAD_H
#define HID_GAMEPAD_H

#include <stdint.h>
#include <stdbool.h>

#define MJ9K_BUTTON_COUNT 48


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


/* //fix to match steel battalion
typedef struct {
    uint64_t buttons;
    int16_t  lx;
    int16_t  ly;
    int16_t  rx;
    int16_t  ry;
    int16_t  z_trigger;
    uint8_t  hat;
    bool     finger_active; // Keep this for touch/trackpads if used elsewhere

    // --- ADD THESE EXPLICIT NATIVE SBC FIELDS ---
    uint8_t  sbc_gear_lever;   // 0 = R, 1 = N, 2 = Gear 1, etc.
    uint8_t  sbc_left_pedal;
    uint8_t  sbc_middle_pedal;
    uint8_t  sbc_right_pedal;
} sbc_data_t; */


/* typedef struct __attribute__((packed)) {
    uint64_t bButtons;
    uint8_t  bRotationLever;
    uint8_t  bSightChangeX;
    uint8_t  bSightChangeY;
    uint8_t  bAimingX;
    uint8_t  bAimingY;
    uint8_t  bLeftPedal;
    uint8_t  bMiddlePedal;
    uint8_t  bRightPedal;
    uint8_t  bTunerDial;
    uint8_t  bGearLever;
} local_sbc_data_t;
 */


typedef struct __attribute__((packed)) {
    uint8_t  zero;           // Padding/Report identifier metadata
    uint8_t  bLength;        // 26-byte tracking packet length
    uint16_t buttons[MJ9K_BUTTON_COUNT / 16]; // 3 blocks of 16-bit packed discrete states
    uint16_t aimingX;        // 0 to 0xFFFF (Left to Right)
    uint16_t aimingY;        // 0 to 0xFFFF (Top to Bottom)
    int16_t  turningLever;   // Signed rotational deviation axis
    int16_t  sightChangeX;   // Signed relative trackball component
    int16_t  sightChangeY;   // Signed relative trackball component
    uint16_t slidePedal;     // Left Pedal (Clutch/Sidestep): 0x0000 to 0xFF00
    uint16_t brakePedal;     // Middle Pedal (Brake): 0x0000 to 0xFF00
    uint16_t accelPedal;     // Right Pedal (Gas): 0x0000 to 0xFF00
    uint8_t  tuner;          // Discrete dial state (0-15 clockwise)
    int8_t   shifter;        // Signed sequential index (-2 to 5)
} local_sbc_data_t;          // Swapped cleanly to match mj9k_in_report





bool parse_ps4_controller(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_generic_hid_gamepad(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_logitech_dual_action(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_steel_battalion_native(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_generic_joystick_interface(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
#endif // HID_GAMEPAD_H
