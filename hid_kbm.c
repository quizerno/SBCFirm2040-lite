#include "hid_kbm.h"
#include "usb_hid_keys.h"
#include "usb_mouse_buttons.h"
#include <stdlib.h>
#include <string.h>

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

void process_hid_mouse(uint8_t const* report, uint16_t len) {
    if (len < 4) return;
    global_mouse_buttons = report[0];
    
    int8_t mx = (int8_t)report[1];
    int8_t my = (int8_t)report[2];
    int8_t mw = (int8_t)report[3];
    
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
    if (target_mouse_action & (MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT | MOUSE_BUTTON_MIDDLE | MOUSE_BUTTON_SIDE1 | MOUSE_BUTTON_SIDE2)) {
        return (local_mouse_buttons & target_mouse_action) != 0;
    }
    if (target_mouse_action == MOUSE_SCROLL_UP) return local_mouse_wheel > 0;
    if (target_mouse_action == MOUSE_SCROLL_DOWN) return local_mouse_wheel < 0;
    return false;
}
