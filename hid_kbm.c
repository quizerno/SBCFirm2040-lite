#include "hid_kbm.h"
#include "usb_hid_keys.h"
#include "usb_mouse_buttons.h"
#include <stdlib.h>
#include <string.h>
#include "tusb.h"


// Instantiating variables defined in header
volatile uint8_t global_modifiers = 0;
volatile uint8_t global_keycodes[6] = {0};
volatile uint8_t global_mouse_buttons = 0;
volatile int32_t global_mouse_x = 0;      
volatile int32_t global_mouse_y = 0;
volatile int8_t  global_mouse_wheel = 0;

uint8_t local_modifiers = 0;
uint8_t local_keycodes[6] = {0};
uint8_t local_mouse_buttons = 0;
int32_t local_mouse_x = 0;
int32_t local_mouse_y = 0;
int8_t  local_mouse_wheel = 0;

void process_hid_keyboard(uint8_t const* report, uint16_t len) {
    if (len < 8) return; // Basic validation check
    global_modifiers = report[0];
    for (int i = 0; i < 6; i++) {
        global_keycodes[i] = report[2 + i];
    }
}

/* void process_hid_mouse(uint8_t const* report, uint16_t len) {
    if (len < 4) return;
    global_mouse_buttons = report[0];
    
    int8_t mx = (int8_t)report[1];
    int8_t my = (int8_t)report[2];
    int8_t mw = (int8_t)report[3];
    
    global_mouse_x += mx;
    global_mouse_y += my;
    global_mouse_wheel += mw;
} */


//WORKING
/* void process_hid_mouse(uint8_t const* report, uint16_t len) {
    // Safety check for empty packets
    if (len < 3) return;

    // Detect if the mouse uses a Report ID prefix (shifts packet bytes by 1)
    uint8_t offset = 0;
    if (len > 4 && report[0] <= 5) { 
        // If the first byte looks like a typical small Report ID, shift expectations
        offset = 1;
    }

    // 1. Map buttons safely using your local masking variables
    global_mouse_buttons = report[0 + offset];
    
    // 2. Safely cast raw unsigned bytes to signed 8-bit tracking deltas
    int8_t mx = (int8_t)report[1 + offset];
    int8_t my = (int8_t)report[2 + offset];
    int8_t mw = 0;
    
    // Check if packet contains wheel data before accessing array bounds
    if ((3 + offset) < len) {
        mw = (int8_t)report[3 + offset];
    }
    
    // 3. Accumulate globally
    global_mouse_x += mx;
    global_mouse_y += my;
    global_mouse_wheel += mw;
}
 */

void process_hid_mouse(uint8_t const* report, uint16_t len) {
    // Drop corrupt or empty packets
    if (len < 3) return;

    // Detect if the mouse uses a Report ID prefix (common on multi-profile mice)
    uint8_t offset = 0;
    if (len > 4 && report[0] <= 5) { 
        offset = 1;
    }

    // 1. Assign buttons safely 
    global_mouse_buttons = report[0 + offset];
    
    // 2. Decode coordinates dynamically based on report payload size
    int8_t mx = 0;
    int8_t my = 0;
    int8_t mw = 0;

    if (len - offset >= 6) {
        // Modern/High-DPI Mouse Format (X and Y occupy 16-bits each)
        // Byte 1 & 2: X-axis, Byte 3 & 4: Y-axis, Byte 5: Scroll Wheel
        mx = (int8_t)report[1 + offset]; // Lower byte slice captures raw delta direction
        my = (int8_t)report[3 + offset]; 
        mw = (int8_t)report[5 + offset];
    } else {
        // Standard Boot Mouse Format (8-bits per axis)
        mx = (int8_t)report[1 + offset];
        my = (int8_t)report[2 + offset];
        
        if ((3 + offset) < len) {
            mw = (int8_t)report[3 + offset];
        }
    }
    
    // 3. Accumulate tracking metrics safely into global spaces
    global_mouse_x += mx;
    global_mouse_y += my;
    global_mouse_wheel += mw;
}



bool is_key_pressed(uint8_t target_macro) {
    if (target_macro == KEY_NONE) return false;
    for (int i = 0; i < 6; i++) {
        if (local_keycodes[i] == target_macro) return true; 
    }
    return false; 
}

bool is_mouse_pressed(uint8_t target_mouse_action) {
    if (target_mouse_action & (MOUSE_BTN_LEFT | MOUSE_BTN_RIGHT | MOUSE_BTN_MIDDLE | MOUSE_BTN_SIDE1 | MOUSE_BTN_SIDE2)) {
        return (local_mouse_buttons & target_mouse_action) != 0;
    }
    if (target_mouse_action == MOUSE_SCROLL_UP) return local_mouse_wheel > 0;
    if (target_mouse_action == MOUSE_SCROLL_DOWN) return local_mouse_wheel < 0;
    return false;
}
