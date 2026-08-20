#include "hid_gamepad.h"
#include <stdlib.h>
#include <string.h>

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

bool parse_generic_hid_gamepad(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    if (len < 3) return false;
    out_data->lx = (int16_t)report[0] - 128;
    out_data->ly = (int16_t)report[1] - 128;
    out_data->rx = (len >= 3) ? ((int16_t)report[2] - 128) : 0;
    out_data->ry = (len >= 4) ? ((int16_t)report[3] - 128) : 0;
    out_data->z_trigger = 0;

    out_data->hat = (len >= 5) ? (report[4] & 0x0F) : 8;
    if (out_data->hat > 8) out_data->hat = 8; // Defensive clamping back to absolute idle release

    out_data->buttons = 0;
    if (len >= 6) out_data->buttons |= ((uint32_t)report[5]);
    if (len >= 7) out_data->buttons |= ((uint32_t)report[6] << 8);
    return true;
}
