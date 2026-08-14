#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>           

#include "pico/stdlib.h"
#include "pico/multicore.h" 
#include "hardware/gpio.h"    
#include "hardware/clocks.h" 

#include "pio_usb.h"        

// --- FORCE COMPILER SCOPE LINKAGE FOR PRIVATE STRUCTURES ---
#include "host/usbh.h"     
#include "host/usbh_pvt.h" 

#include "tusb.h"
#include "tusb_gamepad.h"
#include "usb_hid_keys.h"
#include "usb_mouse_buttons.h"  
#include "input_mapping.h"

#define DEG_2_RAD (3.14159 / 180.0)

// --- HARDWARE CONFIGURATION ---
#define HOST_PIN_DP 0 
#define PICO_LED_PIN 25 // Onboard diagnostic status LED

// --- THREAD-SAFE GLOBAL KEYBOARD DATA ---
volatile uint8_t global_modifiers = 0;
volatile uint8_t global_keycodes[6] = {0};

// --- THREAD-SAFE GLOBAL MOUSE DATA ---
volatile uint8_t global_mouse_buttons = 0;
volatile int8_t  global_mouse_x = 0;
volatile int8_t  global_mouse_y = 0;
volatile int8_t  global_mouse_wheel = 0;

// Local cache buffers populated inside Core 0 to prevent multi-core race conditions
uint8_t local_modifiers = 0;
uint8_t local_keycodes[6] = {0};

uint8_t local_mouse_buttons = 0;
int8_t  local_mouse_x = 0;
int8_t  local_mouse_y = 0;
int8_t  local_mouse_wheel = 0;

/**
 * Thread-safe check function reading from Core 0's isolated cache.
 */
bool is_macro_pressed(uint8_t target_macro) {
    if (target_macro == KEY_NONE) return false;
    for (int i = 0; i < 6; i++) {
        if (local_keycodes[i] == target_macro) {
            return true; 
        }
    }
    return false; 
}

#include "usb_mouse_buttons.h" // Include your new macro definitions

/**
 * Thread-safe check function reading from Core 0's isolated mouse cache.
 */
bool is_mouse_pressed(uint8_t target_mouse_action) {
    // 1. Evaluate standard physical button bitmasks
    if (target_mouse_action & (MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT | MOUSE_BUTTON_MIDDLE | MOUSE_BUTTON_SIDE1 | MOUSE_BUTTON_SIDE2)) {
        return (local_mouse_buttons & target_mouse_action) != 0;
    }

    // 2. Evaluate virtual scrolling vectors
    if (target_mouse_action == MOUSE_SCROLL_UP) {
        return local_mouse_wheel > 0;
    }
    if (target_mouse_action == MOUSE_SCROLL_DOWN) {
        return local_mouse_wheel < 0;
    }

    return false;
}



void update_gamepad(Gamepad *gp);
int t = 0;

// --- CORE 1: DEDICATED HOST CONTROLLER PIPELINE ---
void core1_usb_host_entry() {
    sleep_ms(10);

    // Explicit configuration initialization passed directly to Host stack
    static pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = HOST_PIN_DP;
    tuh_configure(BOARD_HOST_RHPORT_NUM, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    // Initialize Host using modern struct allocation patterns matching your library version
    tusb_rhport_init_t const host_init_config = {
        .role = TUSB_ROLE_HOST
    };
    tusb_init(BOARD_HOST_RHPORT_NUM, &host_init_config);

    while (1) {
        // Process Host background tasks and fire callbacks strictly on Core 1
        tuh_task();
    }
}

int main(void)
{
    // Sysclock should be a multiple of 12MHz for stable low-speed USB timings
    set_sys_clock_khz(120000, true);
    sleep_ms(10); 

    stdio_init_all();

    gpio_init(PICO_LED_PIN);
    gpio_set_dir(PICO_LED_PIN, GPIO_OUT);
    
    // Enable internal weak pull-downs to assist enumeration discovery
    gpio_set_pulls(HOST_PIN_DP, false, true);     // Pull-down D+ (GPIO 0)
    gpio_set_pulls(HOST_PIN_DP + 1, false, true); // Pull-down D- (GPIO 1)

    // VISUAL BOOT FLASH: Confirms Core 0 initialized and is running
    gpio_put(PICO_LED_PIN, 1);
    sleep_ms(150);
    gpio_put(PICO_LED_PIN, 0);

    // Initialize Xbox structures on Native Controller (Port 0)
    init_tusb_gamepad(INPUT_MODE_XBOXORIGINAL);

    // Launch Core 1 to handle host pipeline transfers independently 
    multicore_reset_core1();
    multicore_launch_core1(core1_usb_host_entry);

    // Initialize the native controller stack as an emulator target device
    tusb_rhport_init_t const device_init_config = {
        .role = TUSB_ROLE_DEVICE
    };
    tusb_init(0, &device_init_config);

    Gamepad *gp = gamepad(0);

    while (1)
    {
        // Snapshot Keyboard atomic buffers safely 
        local_modifiers = global_modifiers;
        for (int i = 0; i < 6; i++) {
            local_keycodes[i] = global_keycodes[i];
        }

        // Snapshot Mouse atomic buffers safely
        local_mouse_buttons = global_mouse_buttons;
        local_mouse_x       = global_mouse_x;
        local_mouse_y       = global_mouse_y;
        local_mouse_wheel   = global_mouse_wheel;

        // CRITICAL CLEAR FLUSH: Since mouse data is a relative movement delta, 
        // we reset global registers so movement doesn't endlessly loop.
        global_mouse_x = 0;
        global_mouse_y = 0;
        global_mouse_wheel = 0;

        // 🔬 DIAGNOSTIC LED HOOK:
        // Turns solid ON if ANY valid key data, modifiers, or mouse inputs cross the bridge
        bool key_is_active = false;
        for (int i = 0; i < 6; i++) {
            if (local_keycodes[i] != 0) {
                key_is_active = true;
                break;
            }
        }
        if (key_is_active || local_modifiers != 0 || local_mouse_buttons != 0 || local_mouse_x != 0 || local_mouse_y != 0) {
            gpio_put(PICO_LED_PIN, 1);
        } else {
            gpio_put(PICO_LED_PIN, 0);
        }

        update_gamepad(gp);

        // Keep internal buffers checking modifications 
        tusb_gamepad_task();
        
        // Core 0 handles native Xbox controller output communication pipeline
        tud_task(); 
    }

    return 0;
}

bool is_action_active(MacroMapping_t macro) {
    if (macro.source == INPUT_SRC_KEYBOARD) {
        return is_macro_pressed(macro.code); // Evaluates the 6-key array buffer
    }
    if (macro.source == INPUT_SRC_MOUSE) {
        return is_mouse_pressed(macro.code);   // Evaluates mouse bitmasks / scroll metrics
    }
    return false;
}


void update_gamepad(Gamepad *gp)
{
    memset(&gp->steel_battalion_in_report.dButtons, 0, sizeof(gp->steel_battalion_in_report.dButtons));




    // Map KEY_A and KEY_D macros to the Steer Rotation Lever
    if (is_macro_pressed(KEY_A)) {
        gp->steel_battalion_in_report.rotationLever = -32768; 
    } else if (is_macro_pressed(KEY_D)) {
        gp->steel_battalion_in_report.rotationLever = 32767;  
    } else {
        gp->steel_battalion_in_report.rotationLever = 0;      
    }

    // Map KEY_W and KEY_S macros to vertical aiming pitch
    if (is_macro_pressed(KEY_W)) {
        gp->steel_battalion_in_report.aimingY = 65535; 
    } else if (is_macro_pressed(KEY_S)) {
        gp->steel_battalion_in_report.aimingY = 0;     
    } else {
        gp->steel_battalion_in_report.aimingY = 32767; 
    }

    // Map KEY_RIGHT and KEY_LEFT macros to horizontal aiming yaw
    if (is_macro_pressed(KEY_RIGHT)) {
        gp->steel_battalion_in_report.aimingX = 65535; 
    } else if (is_macro_pressed(KEY_LEFT)) {
        gp->steel_battalion_in_report.aimingX = 0;   
    } else {
        gp->steel_battalion_in_report.aimingX = 32767; 
    }

    // Map Digital Buttons via Cache-safe Macros
	gp->steel_battalion_in_report.aimingX = global_mouse_x;
	gp->steel_battalion_in_report.dButtons.MainWeapon = is_mouse_pressed(MOUSE_BUTTON_LEFT);
	gp->steel_battalion_in_report.dButtons.Fire = is_mouse_pressed(MOUSE_BUTTON_RIGHT);

    gp->steel_battalion_in_report.dButtons.Eject = is_macro_pressed(KEY_SPACE);
    gp->steel_battalion_in_report.dButtons.Ignition = is_macro_pressed(KEY_I);
    gp->steel_battalion_in_report.dButtons.Start = is_macro_pressed(KEY_ENTER);

    // Map Modifier Combinations safely using cached Local variables
    if ((local_modifiers & KEY_MOD_LSHIFT) && is_macro_pressed(KEY_E)) {
        gp->steel_battalion_in_report.dButtons.CockpitHatch = true;
    }

    gp->steel_battalion_in_report.dButtons.Comm1 = is_macro_pressed(KEY_1);
    gp->steel_battalion_in_report.dButtons.Comm2 = is_macro_pressed(KEY_2);
    gp->steel_battalion_in_report.dButtons.Comm3 = is_macro_pressed(KEY_3);
    gp->steel_battalion_in_report.dButtons.Comm4 = is_macro_pressed(KEY_4);
    gp->steel_battalion_in_report.dButtons.Comm5 = is_macro_pressed(KEY_5);

    // Continuous validation checking oscillations (Oscillating pedals test block)
/*     gp->steel_battalion_in_report.leftPedal   = 0xFC00 * sin(t * DEG_2_RAD);
    gp->steel_battalion_in_report.middlePedal = 0xFC00 * sin((t + 60) * DEG_2_RAD);
    gp->steel_battalion_in_report.rightPedal  = 0xFC00 * sin((t + 120) * DEG_2_RAD);
    gp->steel_battalion_in_report.tunerDial   = (int)floor(t / 22.5) % 16; */
    
	
    t++;
}

// --- TINYUSB HOST INTERCEPT CALLBACKS ---
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    (void)desc_report; (void)desc_len;
    
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    
    // EXTENDED ACCESS: Allow both keyboards and mice to activate endpoint streams
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD || itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        tuh_hid_receive_report(dev_addr, instance); 
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void)dev_addr; (void)instance;
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void) len;
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (itf_protocol) 
    {
        case HID_ITF_PROTOCOL_KEYBOARD: {
            hid_keyboard_report_t const * kbd_report = (hid_keyboard_report_t const *) report;
            global_modifiers = kbd_report->modifier;
            for (int i = 0; i < 6; i++) {
                global_keycodes[i] = kbd_report->keycode[i];
            }
            break;
        }

        case HID_ITF_PROTOCOL_MOUSE: {
            hid_mouse_report_t const * mouse_report = (hid_mouse_report_t const *) report;
            global_mouse_buttons = mouse_report->buttons;
            
            // ACCUMULATE RELATIVE DELTAS: Adding values to prevent drops if multiple
            // interrupt packets process within a single Core 0 frame cycle.
            global_mouse_x     += mouse_report->x;
            global_mouse_y     += mouse_report->y;
            global_mouse_wheel += mouse_report->wheel;
            break;
        }

        default:
            break;
    }

    // Continue to request reports from the interrupt endpoint channel
    tuh_hid_receive_report(dev_addr, instance);
}
