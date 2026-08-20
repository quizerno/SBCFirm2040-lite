#include "update_steelbattalion.h"
#include "hid_kbm.h"
#include "usb_hid_keys.h"
#include "usb_mouse_buttons.h"
#include <stdlib.h>
#include <string.h>

#define MOUSE_SENSITIVITY 45

int32_t accumulated_aim_x = 32767;
int32_t accumulated_aim_y = 32767;

void clear_steel_battalion_buttons(Gamepad *gp) {
    memset(&gp->steel_battalion_in_report.dButtons, 0, sizeof(gp->steel_battalion_in_report.dButtons));
}

void apply_keyboard_mouse_to_steel_battalion(Gamepad *gp) {
    // 1. Digital Keyboard Axes Mapping
    if (is_key_pressed(KEY_A)) gp->steel_battalion_in_report.rotationLever = -32768; 
    else if (is_key_pressed(KEY_D)) gp->steel_battalion_in_report.rotationLever = 32767;  
    else gp->steel_battalion_in_report.rotationLever = 0;      

    // 2. Analog Mouse Aiming Vector Integration
    accumulated_aim_x += (local_mouse_x * MOUSE_SENSITIVITY);
    accumulated_aim_y += (local_mouse_y * MOUSE_SENSITIVITY);
    
    if (accumulated_aim_x > 65535) accumulated_aim_x = 65535;
    if (accumulated_aim_x < 0)     accumulated_aim_x = 0;
    if (accumulated_aim_y > 65535) accumulated_aim_y = 65535;
    if (accumulated_aim_y < 0)     accumulated_aim_y = 0;

    gp->steel_battalion_in_report.aimingX = (uint16_t)accumulated_aim_x;
    gp->steel_battalion_in_report.aimingY = (uint16_t)accumulated_aim_y;

    // 3. Button Assignments
    gp->steel_battalion_in_report.dButtons.MainWeapon = is_mouse_pressed(MOUSE_BUTTON_LEFT);
    gp->steel_battalion_in_report.dButtons.Fire       = is_mouse_pressed(MOUSE_BUTTON_RIGHT);
    gp->steel_battalion_in_report.dButtons.Eject      = is_key_pressed(KEY_SPACE);
    gp->steel_battalion_in_report.dButtons.Ignition   = is_key_pressed(KEY_I);
    gp->steel_battalion_in_report.dButtons.Start      = is_key_pressed(KEY_ENTER);
}

void apply_gamepad_to_steel_battalion(generic_gamepad_data_t const* gamepad, Gamepad* gp) {
    // Apply filtering boundaries to prevent drifting centers
    int8_t filtered_x = (abs(gamepad->x) > 15) ? gamepad->x : 0;
    int8_t filtered_y = (abs(gamepad->y) > 15) ? gamepad->y : 0;

    // Direct translation over to 16-bit register envelopes
    gp->steel_battalion_in_report.aimingX = (uint16_t)((filtered_x + 128) << 8);
    gp->steel_battalion_in_report.aimingY = (uint16_t)((filtered_y + 128) << 8);

    // Map unaligned driver bits to specialized virtual controller fields
    if (gamepad->buttons & (1 << 0)) gp->steel_battalion_in_report.dButtons.MainWeapon = true; // Cross
    if (gamepad->buttons & (1 << 1)) gp->steel_battalion_in_report.dButtons.Fire       = true; // Circle
}
