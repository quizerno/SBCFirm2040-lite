#ifndef INPUT_MAPPING_H
#define INPUT_MAPPING_H

#include <stdint.h>
#include <stdbool.h>

// Include your device-specific macro assets
#include "usb_hid_keys.h"
#include "usb_mouse_buttons.h"

// --- INPUT SOURCE IDENTIFICATION FLAGS ---
typedef enum {
    INPUT_SRC_NONE,
    INPUT_SRC_KEYBOARD,
    INPUT_SRC_MOUSE
} InputSourceType_t;

// --- UNIFIED CONTROLLER CONFIGURATION ITEM ---
typedef struct {
    InputSourceType_t source;  // Specifies which physical peripheral to evaluate
    uint8_t           code;    // Holds either the KEY_* or MOUSE_* raw byte macro identifier
} MacroMapping_t;

// --- CONTEXT SEPARATED DECLARATION FUNCTION ---
// Prototype for our unified evaluator function (implemented in main.c)
bool is_action_active(MacroMapping_t macro);

#endif // INPUT_MAPPING_H
