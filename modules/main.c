#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>           

#include "pico/stdlib.h"
#include "pico/multicore.h" 
#include "hardware/gpio.h"    
#include "hardware/clocks.h" 
#include "pico/util/queue.h" 

#include "pio_usb.h"        
#include "tusb.h"

// Segregated driver hooks (Pure parsing only, no Steel Battalion logic inside)
#include "hid_kbm.h"
#include "hid_gamepad.h"
#include "input_mapping.h"

#define HOST_PIN_DP 0 
#define PICO_LED_PIN 25 
#define QUEUE_DEPTH 8
#define MOUSE_SENSITIVITY 45

// Global accumulation frames and timing tracking metrics
int32_t accumulated_aim_x = 32767;
int32_t accumulated_aim_y = 32767;
int t = 0; // State execution cycle timer metric

typedef struct {
    uint8_t dev_addr;
    uint8_t instance;
    bool    needs_activation;
    uint8_t usage_id;  
} ActiveHidDevice_t;

volatile ActiveHidDevice_t device_activation_queue[3] = {0};

typedef struct {
    uint8_t report[64];
    uint16_t len;
    uint8_t usage_id;
    uint8_t dev_addr;
} usb_packet_t;

queue_t gamepad_packet_queue;
void apply_inputs_to_steel_battalion(Gamepad *gp, generic_gamepad_data_t const* pad_data, bool dynamic_pad_active);
void core1_usb_host_entry();

int main(void) {
    set_sys_clock_khz(120000, true);
    sleep_ms(10); 
    stdio_init_all();
    
    queue_init(&gamepad_packet_queue, sizeof(usb_packet_t), QUEUE_DEPTH);

    gpio_init(PICO_LED_PIN);
    gpio_set_dir(PICO_LED_PIN, GPIO_OUT);
    gpio_set_pulls(HOST_PIN_DP, false, true);     
    gpio_set_pulls(HOST_PIN_DP + 1, false, true); 

    init_tusb_gamepad(INPUT_MODE_XBOXORIGINAL);
    multicore_reset_core1();
    multicore_launch_core1(core1_usb_host_entry);

    tusb_rhport_init_t const device_init_config = {.role = TUSB_ROLE_DEVICE};
    tusb_init(0, &device_init_config);
    Gamepad *gp = gamepad(0);
    
    generic_gamepad_data_t local_joy_data = {0};
    bool gamepad_activity = false;

    while (1) {
        // Sync local tracking states from shared global values safely
        local_modifiers = global_modifiers;
        for (int i = 0; i < 6; i++) local_keycodes[i] = global_keycodes[i];
        local_mouse_buttons = global_mouse_buttons;
        local_mouse_x       = global_mouse_x;
        local_mouse_y       = global_mouse_y;
        local_mouse_wheel   = global_mouse_wheel;

        // Clear global delta states for next hardware interrupt tick
        global_mouse_x = 0; global_mouse_y = 0; global_mouse_wheel = 0;
        gamepad_activity = false;

        // Pull asynchronous raw network frames from Core 1
        usb_packet_t pkt;
        if (queue_try_remove(&gamepad_packet_queue, &pkt)) {
            bool parsed_ok = false;
            if (pkt.usage_id == 0x99) { 
                parsed_ok = parse_ps4_controller(pkt.report, pkt.len, &local_joy_data);
            } else if (pkt.usage_id == 0x04 || pkt.usage_id == 0x05) { 
                parsed_ok = parse_generic_hid_gamepad(pkt.report, pkt.len, &local_joy_data);
            }

            if (parsed_ok) {
                gamepad_activity = (local_joy_data.buttons != 0) || (local_joy_data.hat != 0);
            }
        }

        // --- CONSOLIDATED STEEL BATTALION MAPPING MACHINE ---
        apply_inputs_to_steel_battalion(gp, &local_joy_data, gamepad_activity);

        // Core 0 updates activity signaling LED status
        bool key_active = false;
        for (int i = 0; i < 6; i++) { if (local_keycodes[i] != 0) { key_active = true; break; } }
        if (key_active || local_modifiers != 0 || local_mouse_buttons != 0 || gamepad_activity) {
            gpio_put(PICO_LED_PIN, 1);
        } else {
            gpio_put(PICO_LED_PIN, 0);
        }

        tusb_gamepad_task();
        tud_task(); 
    }
    return 0;
}

void apply_inputs_to_steel_battalion(Gamepad *gp, generic_gamepad_data_t const* pad_data, bool dynamic_pad_active) {
    // 1. Wipe old state payload bits out cleanly before frame refresh passes
    memset(&gp->steel_battalion_in_report.dButtons, 0, sizeof(gp->steel_battalion_in_report.dButtons));

    // 2. Keyboard Rotation Lever Mapping
    if (is_key_pressed(KEY_A)) gp->steel_battalion_in_report.rotationLever = -32768; 
    else if (is_key_pressed(KEY_D)) gp->steel_battalion_in_report.rotationLever = 32767;  
    else gp->steel_battalion_in_report.rotationLever = 0;      

    // 3. Mouse Integration Processing Calculations
    accumulated_aim_x += (local_mouse_x * MOUSE_SENSITIVITY);
    accumulated_aim_y += (local_mouse_y * MOUSE_SENSITIVITY);

    // 4. Overlap Analog Stick Data if a Hardware Gamepad is Actively Pushed
    if (dynamic_pad_active) {
        int8_t fx = (abs(pad_data->x) > 15) ? pad_data->x : 0;
        int8_t fy = (abs(pad_data->y) > 15) ? pad_data->y : 0;
        gp->steel_battalion_in_report.aimingX = (uint16_t)((fx + 128) << 8);
        gp->steel_battalion_in_report.aimingY = (uint16_t)((fy + 128) << 8);
        
        // Gamepad direct button overrides mapped onto structural masks
        if (pad_data->buttons & (1 << 0)) gp->steel_battalion_in_report.dButtons.MainWeapon = true; // Cross
        if (pad_data->buttons & (1 << 1)) gp->steel_battalion_in_report.dButtons.Fire       = true; // Circle
    } else {
        // Keyboard mapping fallback configurations
        if (is_key_pressed(KEY_RIGHT)) accumulated_aim_x = 65535;
        else if (is_key_pressed(KEY_LEFT)) accumulated_aim_x = 0;

        if (is_key_pressed(KEY_W)) accumulated_aim_y = 65535;
        else if (is_key_pressed(KEY_S)) accumulated_aim_y = 0;

        if (accumulated_aim_x > 65535) accumulated_aim_x = 65535; if (accumulated_aim_x < 0) accumulated_aim_x = 0;
        if (accumulated_aim_y > 65535) accumulated_aim_y = 65535; if (accumulated_aim_y < 0) accumulated_aim_y = 0;

        gp->steel_battalion_in_report.aimingX = (uint16_t)accumulated_aim_x;
        gp->steel_battalion_in_report.aimingY = (uint16_t)accumulated_aim_y;

        gp->steel_battalion_in_report.dButtons.MainWeapon = is_mouse_pressed(MOUSE_BUTTON_LEFT);
        gp->steel_battalion_in_report.dButtons.Fire       = is_mouse_pressed(MOUSE_BUTTON_RIGHT);
    }

    // 5. System Controls Mapping (Consistent across devices)
    gp->steel_battalion_in_report.dButtons.Eject    = is_key_pressed(KEY_SPACE);
    gp->steel_battalion_in_report.dButtons.Ignition = is_key_pressed(KEY_I);
    gp->steel_battalion_in_report.dButtons.Start    = is_key_pressed(KEY_ENTER);

    gp->steel_battalion_in_report.dButtons.Comm1    = is_key_pressed(KEY_1);
    gp->steel_battalion_in_report.dButtons.Comm2    = is_key_pressed(KEY_2);

    if ((local_modifiers & KEY_MOD_LSHIFT) && is_key_pressed(KEY_E)) {
        gp->steel_battalion_in_report.dButtons.CockpitHatch = true;
    }

    // 6. Increment Time Vector Tracking Metric
    t++;
}

// Keep core1_usb_host_entry(), tuh_hid_mount_cb() hub logic below...
