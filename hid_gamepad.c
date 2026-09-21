#include "hid_gamepad.h"
#include "device_profiles.h"
#include "tusb.h"
#include "host/usbh.h"  
#include <stdlib.h>
#include <string.h>
#include <stdio.h>      


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
    };
    struct {
        uint8_t ps      : 1; 
        uint8_t tpad_click    : 1; 
        uint8_t counter : 6; 
    };
    uint8_t l2_trigger; // Analog Pressure (0 to 255)
    uint8_t r2_trigger; // Analog Pressure (0 to 255)
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

// Formatted system diagnostic display utility over USB-UART Serial
static void debug_dump_raw_report(uint8_t const* report, uint16_t len, const char* name, uint16_t vid, uint16_t pid) {
    printf("\n=== [PICO HOST HID ATTACHMENT REPORT DUMP] ===\n");
    printf("  Target Device : %s\n", name);
    printf("  Hardware ID   : [VID: 0x%04X | PID: 0x%04X]\n", vid, pid);
    printf("  Packet Length : %d Bytes\n", len);
    printf("----------------------------------------------\n  Raw Payload Hex Stream:\n  ");
    
    for (uint16_t i = 0; i < len; i++) {
        printf("%02X ", report[i]);
        if ((i + 1) % 16 == 0 && (i + 1) < len) {
            printf("\n  "); // Align columns perfectly for terminal viewing windows
        }
    }
    printf("\n==============================================\n\n");
}

/**
 * Parses native Steel Battalion data streams.
 * Direct-maps specialized controls without intermediate gamepad conversion errors.
 */
/* bool parse_steel_battalion_native(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    // Structural envelope safety assertion
    if (len < sizeof(steel_battalion_native_report_t)) {
        printf("[WARN] SB Native packet payload undersized: Got %d bytes, expected %d\n", len, sizeof(steel_battalion_native_report_t));
        return false;
    }

    steel_battalion_native_report_t sb_raw;
    memcpy(&sb_raw, report, sizeof(steel_battalion_native_report_t));

    // Clear structural targets safely 
    memset(out_data, 0, sizeof(generic_gamepad_data_t));
    out_data->hat = 8; // Maintain default idle release state marker

    // 1. Forward raw analog components directly to target registers
    // We downscale 16-bit inputs into generic 8-bit envelopes by shifting out noisy bits
    out_data->lx = (int8_t)(sb_raw.aiming_x >> 8);
    out_data->ly = (int8_t)(sb_raw.aiming_y >> 8);
    out_data->rx = (int8_t)(sb_raw.sight_change_x >> 8);
    out_data->ry = (int8_t)(sb_raw.sight_change_y >> 8);

    // 2. Combine foot elements into a singular analog trigger channel
    out_data->z_trigger = (int16_t)sb_raw.accelerator_pedal - (int16_t)sb_raw.brake_pedal;

    // 3. Map digital bits cleanly into uniform output spaces
    out_data->buttons = sb_raw.buttons_block1;

    // Optional Verbose Diagnostic tracking flag (Uncomment to watch real-time inputs over serial)
    // printf("[SB-NATIVE LIVE] Aim X: %4d | Aim Y: %4d | Pedals Accel: %3d\n", sb_raw.aiming_x, sb_raw.aiming_y, sb_raw.accelerator_pedal);

    return true; 
} */

/**
 * Parses native Steel Battalion data streams.
 * Direct-maps specialized controls and dynamically adapts to packet offset variants.
 */
/* bool parse_steel_battalion_native(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    uint8_t const* payload_ptr = report;
    uint16_t expected_size = sizeof(steel_battalion_native_report_t);

    // Defensive check: If the packet has a Report ID prefix, shift our pointer forward by 1 byte
    if (len == expected_size + 1) {
        payload_ptr = &report[1];
    } else if (len < expected_size) {
        printf("[WARN] SB Native packet payload undersized: Got %d bytes, expected %d\n", len, expected_size);
        return false;
    }

    steel_battalion_native_report_t sb_raw;
    memcpy(&sb_raw, payload_ptr, expected_size);

    // Clear structural targets safely 
    memset(out_data, 0, sizeof(generic_gamepad_data_t));
    out_data->hat = 8; // Maintain default idle release state marker

    // 1. Forward raw analog components directly to target registers
    // Downscale 16-bit inputs into generic 8-bit envelopes by shifting out noisy bits
    out_data->lx = (int8_t)(sb_raw.aiming_x >> 8);
    out_data->ly = (int8_t)(sb_raw.aiming_y >> 8);
    out_data->rx = (int8_t)(sb_raw.sight_change_x >> 8);
    out_data->ry = (int8_t)(sb_raw.sight_change_y >> 8);

    // 2. Combine foot elements into a singular analog trigger channel
    out_data->z_trigger = (int16_t)sb_raw.accelerator_pedal - (int16_t)sb_raw.brake_pedal;

    // 3. Map digital bits cleanly into uniform output spaces
    out_data->buttons = sb_raw.buttons_block1;

    return true; 
}
 */
 
 bool parse_steel_battalion_native(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    uint8_t const* payload_ptr = report;
    uint16_t expected_size = sizeof(steel_battalion_native_report_t);

    if (len == expected_size + 1) {
        payload_ptr = &report[1];
    } else if (len < expected_size) {
        return false;
    }

    steel_battalion_native_report_t sb_raw;
    memcpy(&sb_raw, payload_ptr, expected_size);

    memset(out_data, 0, sizeof(generic_gamepad_data_t));
    out_data->hat = 8; 

    // =================================================================
    // NATIVE ENVELOPE TRANSLATION CORRECTION
    // =================================================================
    // If the Arduino mimics standard XInput configurations, the values 
    // are already structured signed values. If they arrive as raw 8-bit 
    // configurations, map them into the generic container cleanly:
    out_data->lx = (int8_t)(sb_raw.aiming_x & 0xFF) - 128;
    out_data->ly = (int8_t)(sb_raw.aiming_y & 0xFF) - 128;
    out_data->rx = (int8_t)(sb_raw.sight_change_x & 0xFF) - 128;
    out_data->ry = (int8_t)(sb_raw.sight_change_y & 0xFF) - 128;

    // 2. Extract Pedal Array Pressures
    out_data->z_trigger = (int16_t)sb_raw.accelerator_pedal - (int16_t)sb_raw.brake_pedal;

    // 3. Map Digital Flags Map
    out_data->buttons = sb_raw.buttons_block1;

    return true; 
}

/**
 * Unpacks raw data frames from the Hori Flightstick PS3/PS4 (VID 0x0F0D, PID 0x00A9).
 * Normalizes large-throw analog pots into our generic internal signed envelopes.
 */
bool parse_hori_flightstick(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    // Basic verification: PlayStation data packets typically carry a Report ID of 0x01
    // A standard Hori flight stick layout carries an explicit data envelope size
    if (len < 10) return false;

    // Accounts for physical Report ID padding shifts if present
    uint8_t offset = (report[0] == 0x01) ? 1 : 0;

    // Clear structure memory states safely
    memset(out_data, 0, sizeof(generic_gamepad_data_t));

    // 1. Map Main Stick Axes (Shift 0..255 unsigned inputs into standard signed center bounds)
    out_data->lx = (int16_t)report[0 + offset] - 128; // Main Stick X (Roll)
    out_data->ly = (int16_t)report[1 + offset] - 128; // Main Stick Y (Pitch)
    
    // 2. Map Auxiliary Analog Controls
    out_data->rx = (int16_t)report[2 + offset] - 128; // Main Stick Twist (Rudder/Yaw)
    out_data->ry = (int16_t)report[3 + offset] - 128; // Physical Throttle Lever Axis

    // 3. Extract the POV Hat Switch (Typically lower 4 bits of byte index 4)
    uint8_t hat_raw = report[4 + offset] & 0x0F;
    out_data->hat = (hat_raw > 7) ? 8 : hat_raw; // Force clamp values outside 0-7 to default IDLE

    // 4. Map the Digital Button Matrix (Unpacking face triggers and grip toggles)
    out_data->buttons = 0;
    
    // Extract upper face buttons packed into the top half of byte index 4
    uint8_t upper_nibble_buttons = (report[4 + offset] >> 4) & 0x0F;
    out_data->buttons |= (upper_nibble_buttons & 0x01) << 0; // Trigger / Primary Fire
    out_data->buttons |= (upper_nibble_buttons & 0x02) << 1; // Missile / Thumb Face 1
    out_data->buttons |= (upper_nibble_buttons & 0x04) << 2; // Weapon Select Face 2
    out_data->buttons |= (upper_nibble_buttons & 0x08) << 3; // Auxiliary Face 3

    // Accumulate structural grip options natively spanning across byte indices 5 and 6
    out_data->buttons |= ((uint32_t)report[5 + offset]) << 4;
    if (len - offset >= 7) {
        out_data->buttons |= ((uint32_t)report[6 + offset]) << 12;
    }

    // Combine specialized analog inputs into your cross-core triggers channel
    // E.g., mapping left and right toe brakes or paddle steps if preferred
    out_data->z_trigger = 0; 
    
    return true;
}



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
    // Continuous Tracking Streams
    out_data->tpad_packets = ds4.tpad_packets;
    out_data->finger_active = (ds4.touch0.active == 0);

    // THE TOUCHPAD ACTIVE BIT FIX:
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

/**
 * Parses raw reports from a Logitech Dual Action Gamepad (VID 046D, PID C216).
 * Safely unpacks stacked hat and button blocks.
 */
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
 * Fallback parser for standard, generic baseline USB HID Gamepads.
 */
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

/**
 * Automated System Router: Identifies connected equipment descriptors,
 * applies profiling parameters, and dumps telemetry cleanly over the UART console.
 */
/* bool route_and_parse_gamepad(uint8_t const* report, uint16_t len, uint8_t dev_addr, generic_gamepad_data_t* out_data) {
    uint16_t vid = 0, pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    gamepad_profile_id_t active_profile = PROFILE_GENERIC_HID;
    const char* active_name = "Unknown Generic Peripheral";

    for (size_t i = 0; i < GAMEPAD_REGISTRY_COUNT; i++) {
        if (GAMEPAD_REGISTRY[i].vid == vid && GAMEPAD_REGISTRY[i].pid == pid) {
            active_profile = GAMEPAD_REGISTRY[i].profile_id;
            active_name = GAMEPAD_REGISTRY[i].name;
            break;
        }
    }

    // Always trigger a diagnostic trace log onto console whenever a generic or debug item cycles data
    if (active_profile == PROFILE_DEBUG_RAW_DUMP) {
        debug_dump_raw_report(report, len, active_name, vid, pid);
        return false; 
    }

    // Execute standard operational decoders
    switch (active_profile) {
        case PROFILE_STEEL_BATTALION:
            return parse_steel_battalion_native(report, len, out_data);

        case PROFILE_SONY_DS4:
            return parse_ps4_controller(report, len, out_data);

        case PROFILE_LOGITECH_DUAL_ACTION:
            return parse_logitech_dual_action(report, len, out_data);

        case PROFILE_GENERIC_HID:
        default:
            return parse_generic_hid_gamepad(report, len, out_data);
    }
} */

/* bool route_and_parse_gamepad(uint8_t const* report, uint16_t len, uint8_t dev_addr, generic_gamepad_data_t* out_data) {
    uint16_t vid = 0, pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    // FORCE DIRECT OVERRIDE: Check for your emulated Steel Battalion controllers immediately
    if ((vid == 0x0A7B && pid == 0xD000) || (vid == 0x045E && pid == 0x0289)) {
        return parse_steel_battalion_native(report, len, out_data);
    }

    // Otherwise, maintain normal lookup rules for your commercial pads
    gamepad_profile_id_t active_profile = PROFILE_GENERIC_HID;
    for (size_t i = 0; i < GAMEPAD_REGISTRY_COUNT; i++) {
        if (GAMEPAD_REGISTRY[i].vid == vid && GAMEPAD_REGISTRY[i].pid == pid) {
            active_profile = GAMEPAD_REGISTRY[i].profile_id;
            break;
        }
    }

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
 */
 
/*  bool route_and_parse_gamepad(uint8_t const* report, uint16_t len, uint8_t dev_addr, generic_gamepad_data_t* out_data) {
    uint16_t vid = 0, pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid); // Safe to run on Core 0 thread context here!

    // DIRECT INTERCEPT: Route your emulated Steel Battalion controllers immediately
    if ((vid == 0x0A7B && pid == 0xD000) || (vid == 0x045E && pid == 0x0289)) {
        return parse_steel_battalion_native(report, len, out_data);
    }

    // Maintain normal lookup rules for your commercial pads
    gamepad_profile_id_t active_profile = PROFILE_GENERIC_HID;
    for (size_t i = 0; i < GAMEPAD_REGISTRY_COUNT; i++) {
        if (GAMEPAD_REGISTRY[i].vid == vid && GAMEPAD_REGISTRY[i].pid == pid) {
            active_profile = GAMEPAD_REGISTRY[i].profile_id;
            break;
        }
    }

    switch (active_profile) {
        case PROFILE_SONY_DS4:
            return parse_ps4_controller(report, len, out_data);
        case PROFILE_LOGITECH_DUAL_ACTION:
            return parse_logitech_dual_action(report, len, out_data);
        case PROFILE_GENERIC_HID:
        default:
            return parse_generic_hid_gamepad(report, len, out_data);
    }
} */


bool route_and_parse_gamepad(uint8_t const* report, uint16_t len, uint8_t dev_addr, generic_gamepad_data_t* out_data) {
    uint16_t vid = 0, pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid); // Safe to run on Core 0 thread context here!

    // DIRECT INTERCEPT: Route your emulated Steel Battalion controllers immediately
    if ((vid == 0x0A7B && pid == 0xD000) || (vid == 0x045E && pid == 0x0289)) {
        return parse_steel_battalion_native(report, len, out_data);
    }

    // Maintain normal lookup rules for your commercial pads
    gamepad_profile_id_t active_profile = PROFILE_GENERIC_HID;
    bool found_in_registry = false;

    for (size_t i = 0; i < GAMEPAD_REGISTRY_COUNT; i++) {
        if (GAMEPAD_REGISTRY[i].vid == vid && GAMEPAD_REGISTRY[i].pid == pid) {
            active_profile = GAMEPAD_REGISTRY[i].profile_id;
            found_in_registry = true;
            break;
        }
    }


    // Replace the bottom section of route_and_parse_gamepad with this:
    switch (active_profile) {
        case PROFILE_HORI_FLIGHTSTICK:
            return parse_hori_flightstick(report, len, out_data);
        case PROFILE_SONY_DS4:
            return parse_ps4_controller(report, len, out_data);
            
        case PROFILE_LOGITECH_DUAL_ACTION:
            return parse_logitech_dual_action(report, len, out_data);
            
        case PROFILE_GENERIC_HID:
        default:
            return parse_generic_hid_gamepad(report, len, out_data);
    }

}



/**
 * Unpacks standard, generic baseline USB HID Joysticks (Usage 0x05 / Gamepads).
 * Extracts raw coordinate axes stream arrays and pushes them to standard formats.
 */
bool parse_generic_joystick_interface(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data) {
    if (len < 3) return false;

    // Detect if multi-profile composite framing shifted our array columns by 1 byte
    uint8_t offset = 0;
    if (len > 4 && report[0] <= 5) { 
        offset = 1;
    }

    // 1. Unpack default directional coordinate fields with your dynamic offset bounds
    out_data->lx = (int16_t)report[0 + offset] - 128;
    out_data->ly = (int16_t)report[1 + offset] - 128;
    
    out_data->rx = (len - offset >= 3) ? ((int16_t)report[2 + offset] - 128) : 0;
    out_data->ry = (len - offset >= 4) ? ((int16_t)report[3 + offset] - 128) : 0;
    out_data->z_trigger = 0;

    // 2. Unpack Directional POV Hat Switch if embedded inside index byte 4
    if (len - offset >= 5) {
        uint8_t hat_raw = report[4 + offset] & 0x0F;
        out_data->hat = (hat_raw > 7) ? 8 : hat_raw; 
    } else {
        out_data->hat = 8;
    }

    // 3. Extract Digital Action Matrix bits safely across indices 5 and 6
    out_data->buttons = 0;
    if (len - offset >= 6) out_data->buttons |= ((uint32_t)report[5 + offset]);
    if (len - offset >= 7) out_data->buttons |= ((uint32_t)report[6 + offset] << 8);

    out_data->tpad_packets = 0;
    out_data->finger_active = false;

    return true;
}


