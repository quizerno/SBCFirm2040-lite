#ifndef UPDATE_STEELBATTALION_H
#define UPDATE_STEELBATTALION_H

#include "tusb_gamepad.h"
#include "hid_gamepad.h"

// Expose variables used for accumulation processing mechanics
extern int32_t accumulated_aim_x;
extern int32_t accumulated_aim_y;

// Operational translation procedures
void clear_steel_battalion_buttons(Gamepad *gp);
void apply_keyboard_mouse_to_steel_battalion(Gamepad *gp);
void apply_gamepad_to_steel_battalion(generic_gamepad_data_t const* gamepad, Gamepad* gp);

#endif // UPDATE_STEELBATTALION_H
