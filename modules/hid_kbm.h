#ifndef HID_KBM_H
#define HID_KBM_H

#include stdint.h
#include stdbool.h

 Global structures shared with main execution loop
extern volatile uint8_t global_modifiers;
extern volatile uint8_t global_keycodes[6];
extern volatile uint8_t global_mouse_buttons;
extern volatile int32_t global_mouse_x;      
extern volatile int32_t global_mouse_y;
extern volatile int8_t  global_mouse_wheel;

extern uint8_t local_modifiers;
extern uint8_t local_keycodes[6];
extern uint8_t local_mouse_buttons;
extern uint8_t local_mouse_x;
extern uint8_t local_mouse_y;
extern int8_t  local_mouse_wheel;

 Functions migrated from old main structure
void process_hid_keyboard(uint8_t const report, uint16_t len);
void process_hid_mouse(uint8_t const report, uint16_t len);
bool is_key_pressed(uint8_t target_macro);
bool is_mouse_pressed(uint8_t target_mouse_action);

#endif  HID_KBM_H
