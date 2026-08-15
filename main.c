#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>           

#include "pico/stdlib.h"
#include "pico/multicore.h" 
#include "hardware/gpio.h"    
#include "hardware/clocks.h" 

#include "pio_usb.h"        

#include "host/usbh.h"     
#include "host/usbh_pvt.h" 

#include "tusb.h"
#include "tusb_gamepad.h"
#include "usb_hid_keys.h"
#include "usb_mouse_buttons.h"  
#include "input_mapping.h"

#define DEG_2_RAD (3.14159 / 180.0)
#define HOST_PIN_DP 0 
#define PICO_LED_PIN 25 

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

int32_t accumulated_aim_x = 32767;
int32_t accumulated_aim_y = 32767;
#define MOUSE_SENSITIVITY 45

typedef struct {
    uint8_t dev_addr;
    uint8_t instance;
    bool    needs_activation;
    uint8_t usage_id;  
} ActiveHidDevice_t;

volatile ActiveHidDevice_t device_activation_queue[3] = {0};

bool is_macro_pressed(uint8_t target_macro) {
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

void update_gamepad(Gamepad *gp);
int t = 0;

void core1_usb_host_entry() {
    sleep_ms(10);
    static pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = HOST_PIN_DP;
    tuh_configure(BOARD_HOST_RHPORT_NUM, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    tusb_rhport_init_t const host_init_config = {.role = TUSB_ROLE_HOST};
    tusb_init(BOARD_HOST_RHPORT_NUM, &host_init_config);

    while (1) {
        tuh_task();
        for (int i = 0; i < 3; i++) {
            if (device_activation_queue[i].needs_activation) {
                uint8_t addr = device_activation_queue[i].dev_addr;
                uint8_t inst = device_activation_queue[i].instance;
                device_activation_queue[i].needs_activation = false;
                tuh_hid_receive_report(addr, inst);
            }
        }
    }
}

int main(void) {
    set_sys_clock_khz(120000, true);
    sleep_ms(10); 
    stdio_init_all();
    gpio_init(PICO_LED_PIN);
    gpio_set_dir(PICO_LED_PIN, GPIO_OUT);
    gpio_set_pulls(HOST_PIN_DP, false, true);     
    gpio_set_pulls(HOST_PIN_DP + 1, false, true); 

    gpio_put(PICO_LED_PIN, 1);
    sleep_ms(150);
    gpio_put(PICO_LED_PIN, 0);

    init_tusb_gamepad(INPUT_MODE_XBOXORIGINAL);
    multicore_reset_core1();
    multicore_launch_core1(core1_usb_host_entry);

    tusb_rhport_init_t const device_init_config = {.role = TUSB_ROLE_DEVICE};
    tusb_init(0, &device_init_config);
    Gamepad *gp = gamepad(0);

    while (1) {
        local_modifiers = global_modifiers;
        for (int i = 0; i < 6; i++) local_keycodes[i] = global_keycodes[i];

        local_mouse_buttons = global_mouse_buttons;
        local_mouse_x       = global_mouse_x;
        local_mouse_y       = global_mouse_y;
        local_mouse_wheel   = global_mouse_wheel;

        global_mouse_x = 0;
        global_mouse_y = 0;
        global_mouse_wheel = 0;

        bool key_is_active = false;
        for (int i = 0; i < 6; i++) {
            if (local_keycodes[i] != 0) { key_is_active = true; break; }
        }
        if (key_is_active || local_modifiers != 0 || local_mouse_buttons != 0 || local_mouse_x != 0 || local_mouse_y != 0) {
            gpio_put(PICO_LED_PIN, 1);
        } else {
            gpio_put(PICO_LED_PIN, 0);
        }

        update_gamepad(gp);
        tusb_gamepad_task();
        tud_task(); 
    }
    return 0;
}
bool is_action_active(MacroMapping_t macro) {
    if (macro.source == INPUT_SRC_KEYBOARD) return is_macro_pressed(macro.code);
    if (macro.source == INPUT_SRC_MOUSE) return is_mouse_pressed(macro.code);
    return false;
}

void update_gamepad(Gamepad *gp) {
    memset(&gp->steel_battalion_in_report.dButtons, 0, sizeof(gp->steel_battalion_in_report.dButtons));

    if (is_macro_pressed(KEY_A)) {
        gp->steel_battalion_in_report.rotationLever = -32768; 
    } else if (is_macro_pressed(KEY_D)) {
        gp->steel_battalion_in_report.rotationLever = 32767;  
    } else {
        gp->steel_battalion_in_report.rotationLever = 0;      
    }

    accumulated_aim_x += (local_mouse_x * MOUSE_SENSITIVITY);
    accumulated_aim_y += (local_mouse_y * MOUSE_SENSITIVITY);

    if (accumulated_aim_x > 65535) accumulated_aim_x = 65535;
    if (accumulated_aim_x < 0)     accumulated_aim_x = 0;
    if (accumulated_aim_y > 65535) accumulated_aim_y = 65535;
    if (accumulated_aim_y < 0)     accumulated_aim_y = 0;

    if (is_macro_pressed(KEY_RIGHT)) {
        gp->steel_battalion_in_report.aimingX = 65535; 
    } else if (is_macro_pressed(KEY_LEFT)) {
        gp->steel_battalion_in_report.aimingX = 0;   
    } else {
        gp->steel_battalion_in_report.aimingX = (uint16_t)accumulated_aim_x; 
    }

    if (is_macro_pressed(KEY_W)) {
        gp->steel_battalion_in_report.aimingY = 65535; 
    } else if (is_macro_pressed(KEY_S)) {
        gp->steel_battalion_in_report.aimingY = 0;     
    } else {
        gp->steel_battalion_in_report.aimingY = (uint16_t)accumulated_aim_y; 
    }

    gp->steel_battalion_in_report.dButtons.MainWeapon = is_mouse_pressed(MOUSE_BUTTON_LEFT);
    gp->steel_battalion_in_report.dButtons.Fire       = is_mouse_pressed(MOUSE_BUTTON_RIGHT);

    gp->steel_battalion_in_report.dButtons.Eject    = is_macro_pressed(KEY_SPACE);
    gp->steel_battalion_in_report.dButtons.Ignition = is_macro_pressed(KEY_I);
    gp->steel_battalion_in_report.dButtons.Start    = is_macro_pressed(KEY_ENTER);

    if ((local_modifiers & KEY_MOD_LSHIFT) && is_macro_pressed(KEY_E)) {
        gp->steel_battalion_in_report.dButtons.CockpitHatch = true;
    }

    gp->steel_battalion_in_report.dButtons.Comm1 = is_macro_pressed(KEY_1);
    gp->steel_battalion_in_report.dButtons.Comm2 = is_macro_pressed(KEY_2);
    gp->steel_battalion_in_report.dButtons.Comm3 = is_macro_pressed(KEY_3);
    gp->steel_battalion_in_report.dButtons.Comm4 = is_macro_pressed(KEY_4);
    gp->steel_battalion_in_report.dButtons.Comm5 = is_macro_pressed(KEY_5);
    t++;
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    uint8_t usage_id = 0;

    if (itf_protocol == HID_ITF_PROTOCOL_NONE) {
        tuh_hid_report_info_t report_info[3]; 
        uint8_t report_count = tuh_hid_parse_report_descriptor(report_info, 3, desc_report, desc_len);
        for (uint8_t i = 0; i < report_count; i++) {
            if (report_info[i].usage_page == 0x01) { 
                if (report_info[i].usage == 0x04 || report_info[i].usage == 0x05) {
                    usage_id = report_info[i].usage;
                    break;
                }
            }
        }
        if (usage_id == 0) return; 
    }

    for (int i = 0; i < 3; i++) {
        if (device_activation_queue[i].dev_addr == 0) {
            device_activation_queue[i].dev_addr = dev_addr;
            device_activation_queue[i].instance = instance;
            device_activation_queue[i].usage_id = usage_id; 
            device_activation_queue[i].needs_activation = true; 
            break;
        }
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    for (int i = 0; i < 3; i++) {
        if (device_activation_queue[i].dev_addr == dev_addr && device_activation_queue[i].instance == instance) {
            device_activation_queue[i].dev_addr = 0;
            device_activation_queue[i].instance = 0;
            device_activation_queue[i].usage_id = 0;
            device_activation_queue[i].needs_activation = false;
            break;
        }
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    (void) len;
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        hid_keyboard_report_t const * kbd_report = (hid_keyboard_report_t const *) report;
        global_modifiers = kbd_report->modifier;
        for (int i = 0; i < 6; i++) global_keycodes[i] = kbd_report->keycode[i];
    }
    else if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        hid_mouse_report_t const * m_rep = (hid_mouse_report_t const *) report;
        global_mouse_buttons = m_rep->buttons;
        
        int32_t mx = m_rep->x;
        int32_t my = m_rep->y;
        int8_t mw = m_rep->wheel;
        
        global_mouse_x = global_mouse_x + mx;
        global_mouse_y = global_mouse_y + my;
        global_mouse_wheel = global_mouse_wheel + mw;
    }
    else if (itf_protocol == HID_ITF_PROTOCOL_NONE) {
        uint8_t active_usage = 0;
        for (int i = 0; i < 3; i++) {
            if (device_activation_queue[i].dev_addr == dev_addr && device_activation_queue[i].instance == instance) {
                active_usage = device_activation_queue[i].usage_id;
                break;
            }
        }
        if (active_usage == 0x05) { /* Gamepad placeholder */ } 
        else if (active_usage == 0x04) { /* Flightstick placeholder */ }
    }
    tuh_hid_receive_report(dev_addr, instance);
}
