#include "device_profiles.h"
#include "hid_gamepad.h"
#include <stdlib.h>
#include <string.h>
#include "host/usbh.h"

typedef struct TU_ATTR_PACKED {
    uint8_t lx;       // Sony tracks Left Stick X here
    uint8_t ly;       // Sony tracks Left Stick Y here
    uint8_t rx;       // Sony tracks Right Stick X here
    uint8_t ry;       // Sony tracks Right Stick Y here
    struct {
        uint8_t dpad     : 4; // 0-7 = directional presses, 8 = IDLE
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
		//uint8_t tpad_click     : 1;
    };
    struct {
        uint8_t ps      : 1; 
        uint8_t tpad_click    : 1; 
        uint8_t counter : 6; 
    };
    uint8_t l2_trigger; // Analog Pressure (0 to 255)
    uint8_t r2_trigger; // Analog Pressure (0 to 255)
	// Touch tracking sequence frame parameters
    uint8_t tpad_packets; 
    
    struct {
        uint8_t id       : 7; 
        uint8_t active   : 1; // 0 = Touch tracking active, 1 = Idle
        uint8_t x_low;        
        uint8_t x_high   : 4; 
        uint8_t y_low    : 4; 
        uint8_t y_high;       
    } touch0;
	
	
} sony_ds4_report_t;

bool parse_ps4_controller(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    // Basic defensive boundary check: Verify Report ID is 1 and data size is safe
    if (report[0] != 1 || len < sizeof(sony_ds4_report_t) + 1) return false;
    
    sony_ds4_report_t ds4;
    memcpy(&ds4, &report[1], sizeof(sony_ds4_report_t));

    // 1. Map Left and Right Sticks: Convert unsigned offset (0..255) to standard signed bounds
    out_data->lx = (int16_t)ds4.lx - 128;
    out_data->ly = (int16_t)ds4.ly - 128;
    out_data->rx = (int16_t)ds4.rx - 128;
    out_data->ry = (int16_t)ds4.ry - 128;
    
    // 2. Map Z Trigger Axis: Combine both pressure outputs into a unified steering variable
    out_data->z_trigger = (int16_t)ds4.r2_trigger - (int16_t)ds4.l2_trigger;

    // 3. FIX DPAD: Pass the raw 4-bit state directly. 8 means idle, no buttons pressed.
    out_data->hat = ds4.dpad;

    // 4. Map Digital Action Switches
    out_data->buttons = 0;
    out_data->buttons |= (ds4.cross    << 0); 
    out_data->buttons |= (ds4.circle   << 1); 
    out_data->buttons |= (ds4.square   << 2); 
    out_data->buttons |= (ds4.triangle << 3); 
    out_data->buttons |= (ds4.l1       << 4);
    out_data->buttons |= (ds4.r1       << 5);
    out_data->buttons |= (ds4.share    << 6);
    out_data->buttons |= (ds4.option   << 7);
    out_data->buttons |= (ds4.ps       << 8);
	out_data->buttons |= (ds4.l3       << 9);
	out_data->buttons |= (ds4.r3       << 10);
	out_data->buttons |= (ds4.tpad_click << 11);
	
	// CONSOLIDATION: Inject physical touchpad press into bit index 11
    // 2. Continuous Tracking Streams
    out_data->tpad_packets = ds4.tpad_packets;
    out_data->finger_active = (ds4.touch0.active == 0);


    // 4. THE TOUCHPAD ACTIVE BIT FIX:
    // In a standard USB report, the first finger status byte sits at exactly index 35.
    // Bit 7 of this byte is the active bit: 0 = Touched, 1 = Idle.
    uint8_t finger0_status_byte = report[35];
    
    // We isolate Bit 7 (0x80). If it equals 0, a finger is physically present!
    if ((finger0_status_byte & 0x80) == 0) {
        out_data->finger_active = true;
        
        // Extract 12-bit X and Y coordinates directly from the adjacent array slots
        out_data->finger_x = report[36] | ((report[37] & 0x0F) << 8);
        out_data->finger_y = ((report[37] & 0xF0) >> 4) | (report[38] << 4);
    } else {
        out_data->finger_active = false;
        out_data->finger_x = 0;
        out_data->finger_y = 0;
    }

    // Extract the sequential transaction loop counter byte (Byte 34)
    out_data->tpad_packets = report[34];
	
    return true;
}

// Updated generic parsing function to specifically handle Logitech layouts
bool parse_generic_hid_gamepad(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    // The Logitech Dual Action sends a 6-byte or 8-byte report packet
    if (len < 6) return false;

    // 1. Map Left and Right Sticks: Convert unsigned offset (0..255) to signed (-128..127)
    out_data->lx = (int16_t)report[0] - 128;
    out_data->ly = (int16_t)report[1] - 128;
    out_data->rx = (int16_t)report[2] - 128;
    out_data->ry = (int16_t)report[3] - 128;
    
    out_data->z_trigger = 0; // The Logitech Dual Action does not have analog triggers

    // 2. Parse Hat Switch (D-Pad)
    // On the Logitech, the lower 4 bits of Byte 4 contain the Hat Switch state.
    // 0 = North, 1 = North-East, 2 = East, ..., 7 = North-West, 15 (or 8) = Idle Release
    uint8_t hat_raw = report[4] & 0x0F;
    if (hat_raw > 7) {
        out_data->hat = 8; // Safely force to absolute idle release
    } else {
        out_data->hat = hat_raw;
    }

    // 3. Map Digital Buttons (Logitech features 12 digital face switches/triggers)
    out_data->buttons = 0;

    // Buttons 1-4 are packed into the high nibble (upper 4 bits) of Byte 4
    uint8_t upper_buttons = (report[4] >> 4) & 0x0F; 
    out_data->buttons |= (upper_buttons & 0x01) << 0; // Button 1 (X / Cross equivalent)
    out_data->buttons |= (upper_buttons & 0x02) << 1; // Button 2 (A / Circle equivalent)
    out_data->buttons |= (upper_buttons & 0x04) << 2; // Button 3 (B / Square equivalent)
    out_data->buttons |= (upper_buttons & 0x08) << 3; // Button 4 (Y / Triangle equivalent)

    // Buttons 5-12 occupy the entirety of Byte 5
    uint8_t byte5_buttons = report[5];
    out_data->buttons |= (uint32_t)byte5_buttons << 4; // Shifts Buttons 5-12 cleanly into bits 4-11

    return true;
}

// ... Keep your existing parse_ps4_controller and parse_generic_hid_gamepad here ...

// Modular parser dedicated strictly to the Logitech Dual Action layout
bool parse_logitech_dual_action(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    if (len < 6) return false;

    // 1. Map Axes (Unsigned 8-bit to Signed 8-bit offsets)
    out_data->lx = (int16_t)report[0] - 128;
    out_data->ly = (int16_t)report[1] - 128;
    out_data->rx = (int16_t)report[2] - 128;
    out_data->ry = (int16_t)report[3] - 128;
    out_data->z_trigger = 0; // Digital buttons only, no analog triggers

    // 2. Map Hat Switch (Lower 4 bits of Byte 4)
    uint8_t hat_raw = report[4] & 0x0F;
    out_data->hat = (hat_raw > 7) ? 8 : hat_raw;

    // 3. Map Digital Action Buttons
    out_data->buttons = 0;

    // Buttons 1-4 are packed into the high nibble of Byte 4
    uint8_t upper_buttons = (report[4] >> 4) & 0x0F; 
    out_data->buttons |= (upper_buttons & 0x01) << 0; // Button 1
    out_data->buttons |= (upper_buttons & 0x02) << 1; // Button 2
    out_data->buttons |= (upper_buttons & 0x04) << 2; // Button 3
    out_data->buttons |= (upper_buttons & 0x08) << 3; // Button 4

    // Buttons 5-12 occupy the entirety of Byte 5
    uint8_t byte5_buttons = report[5];
    out_data->buttons |= (uint32_t)byte5_buttons << 4; // Shift into bits 4-11

    // Clear touchpad fields since this hardware lacks touch support
    out_data->tpad_packets = 0;
    out_data->finger_active = false;
    out_data->finger_x = 0;
    out_data->finger_y = 0;

    return true;
}
/**
 * Automatically identifies a connected gamepad device and executes its matching 
 * hardware-specific parsing strategy.
 */
bool route_and_parse_gamepad(uint8_t const* report, uint16_t len, uint8_t dev_addr, generic_gamepad_data_t* out_data) {
    uint16_t vid = 0, pid = 0;
    
    // Extract hardware descriptors from the active TinyUSB stack instance
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    // Look up the active profiling layout match from our registry
    gamepad_profile_id_t active_profile = PROFILE_GENERIC_HID;
    for (size_t i = 0; i < GAMEPAD_REGISTRY_COUNT; i++) {
        if (GAMEPAD_REGISTRY[i].vid == vid && GAMEPAD_REGISTRY[i].pid == pid) {
            active_profile = GAMEPAD_REGISTRY[i].profile_id;
            break;
        }
    }

    // Execute decoding pipeline based on selected tracking profile
    switch (active_profile) {
        case PROFILE_SONY_DS4:
            return parse_ps4_controller(report, len, out_data);

        case PROFILE_LOGITECH_DUAL_ACTION:
            return parse_logitech_dual_action(report, len, out_data);

        case PROFILE_GENERIC_HID:
        default:
            return parse_generic_hid_gamepad(report, len, out_data);
    }
}