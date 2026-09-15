
#include "class/sbc/sbc_host.h"
#include "device_profiles.h"
#include "tusb_gamepad.h"  
#include "input_mapping.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "host/usbh_pvt.h" // Needed for usbh_class_driver_t           

#include "pico/stdlib.h"
#include "pico/multicore.h" 
#include "hardware/gpio.h"    
#include "hardware/clocks.h" 
#include "pico/util/queue.h" 

#include "pio_usb.h"        
#include "tusb.h"

#include "hid_kbm.h"
#include "hid_gamepad.h"
#include "neopixel.h"
#include "usb_host_callbacks.h" // Include modular host callback structures

#define HOST_PIN_DP 0 
#define PICO_LED_PIN 25 
#define QUEUE_DEPTH 8
#define MOUSE_SENSITIVITY 45
#define fireButtonPin 15

int32_t accumulated_aim_x = 32767;
int32_t accumulated_aim_y = 32767;
int t = 0; 

queue_t gamepad_packet_queue;
void apply_inputs_to_steel_battalion(Gamepad *gp, generic_gamepad_data_t const* pad_data, bool dynamic_pad_active);
void core1_usb_host_entry();

int main(void) {
    set_sys_clock_khz(120000, true);
    sleep_ms(10); 
    stdio_init_all();
    
    neopixel_init();
    neopixel_set_color(20, 20, 20);	
	
    queue_init(&gamepad_packet_queue, sizeof(usb_packet_t), QUEUE_DEPTH);

    gpio_init(PICO_LED_PIN);
    gpio_set_dir(PICO_LED_PIN, GPIO_OUT);
    gpio_set_pulls(HOST_PIN_DP, false, true);     
    gpio_set_pulls(HOST_PIN_DP + 1, false, true); 
	
    gpio_init(fireButtonPin);
    gpio_set_dir(fireButtonPin, GPIO_IN);
    gpio_pull_up(fireButtonPin); 

    init_tusb_gamepad(INPUT_MODE_XBOXORIGINAL);
    multicore_reset_core1();
    multicore_launch_core1(core1_usb_host_entry);

    tusb_rhport_init_t const device_init_config = {.role = TUSB_ROLE_DEVICE};
    tusb_init(0, &device_init_config);
    Gamepad *gp = gamepad(0);
    
    generic_gamepad_data_t local_joy_data = {0};
    bool gamepad_activity = false;

    while (1) {
        local_modifiers     = global_modifiers;
        for (int i = 0; i < 6; i++) local_keycodes[i] = global_keycodes[i];
        local_mouse_buttons = global_mouse_buttons;
        local_mouse_x       = global_mouse_x;
        local_mouse_y       = global_mouse_y;
        local_mouse_wheel   = global_mouse_wheel;

        global_mouse_x = 0; 
        global_mouse_y = 0; 
        gamepad_activity = false;
        global_mouse_wheel = 0;

        memset(&local_joy_data, 0, sizeof(generic_gamepad_data_t));
        local_joy_data.hat = 8; 

        usb_packet_t pkt;
        if (queue_try_remove(&gamepad_packet_queue, &pkt)) {
            bool parsed_ok = route_and_parse_gamepad(pkt.report, pkt.len, pkt.dev_addr, &local_joy_data);

            if (parsed_ok) {
                bool button_or_hat_active = (local_joy_data.buttons != 0) || (local_joy_data.hat != 8);
                bool touch_active = (local_joy_data.finger_active);
                bool analog_sticks_active = (abs(local_joy_data.lx) > 15) || (abs(local_joy_data.ly) > 15) ||
                                            (abs(local_joy_data.rx) > 15) || (abs(local_joy_data.ry) > 15);
                bool z_axis_active = (local_joy_data.z_trigger != 0);
				
                gamepad_activity = button_or_hat_active || analog_sticks_active || z_axis_active || touch_active;
            }
        }

        apply_inputs_to_steel_battalion(gp, &local_joy_data, gamepad_activity);

        bool key_active = false;
        for (int i = 0; i < 6; i++) { if (local_keycodes[i] != 0) { key_active = true; break; } }
		bool mouse_active = local_mouse_buttons != 0 || local_mouse_x != 0 || local_mouse_y != 0 || local_mouse_wheel != 0;
		bool GPIOActive = (gpio_get(fireButtonPin) == 0);
		
        if (key_active || local_modifiers != 0 || mouse_active || gamepad_activity || GPIOActive) {
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
    memset(&gp->steel_battalion_in_report.dButtons, 0, sizeof(gp->steel_battalion_in_report.dButtons));
	
    if (pad_data->hat == 0)   gp->steel_battalion_in_report.sightChangeY = -8000;
    if (pad_data->hat == 4)   gp->steel_battalion_in_report.sightChangeY = 8000;
    if (pad_data->hat == 2)	  gp->steel_battalion_in_report.sightChangeX = 8000;
    if (pad_data->hat == 6)	  gp->steel_battalion_in_report.sightChangeY = -8000;
	
    if (is_key_pressed(KEY_A)) gp->steel_battalion_in_report.rotationLever = -32768; 
    else if (is_key_pressed(KEY_D)) gp->steel_battalion_in_report.rotationLever = 32767;  
    else gp->steel_battalion_in_report.rotationLever = 0;      

    accumulated_aim_x += (local_mouse_x * MOUSE_SENSITIVITY);
    accumulated_aim_y += (local_mouse_y * MOUSE_SENSITIVITY);
	
	if(gpio_get(fireButtonPin)==0){
	    gp->steel_battalion_in_report.dButtons.MainWeapon = true;
	}

    if (dynamic_pad_active) {
        int16_t fx = pad_data->lx;
        int16_t fy = pad_data->ly;
        int16_t rx = pad_data->rx;
        int16_t ry = pad_data->ry;
        
        gp->steel_battalion_in_report.aimingX = (uint16_t)((fx + 128) << 8);
        gp->steel_battalion_in_report.aimingY = (uint16_t)((fy + 128) << 8);
        
        gp->steel_battalion_in_report.sightChangeX = (uint16_t)((rx + 128) << 8);
        gp->steel_battalion_in_report.sightChangeY = (uint16_t)((ry + 128) << 8);
        
        if (pad_data->buttons & (1 << 0)) gp->steel_battalion_in_report.dButtons.MainWeapon = true; 
        if (pad_data->buttons & (1 << 1)) gp->steel_battalion_in_report.dButtons.Fire       = true; 
        if (pad_data->buttons & (1 << 3)) gp->steel_battalion_in_report.dButtons.LockOn       = true; 
        
    } else {		
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

    gp->steel_battalion_in_report.dButtons.CockpitHatch    = is_key_pressed(KEY_P);
    gp->steel_battalion_in_report.dButtons.Eject    = is_key_pressed(KEY_SPACE);
    gp->steel_battalion_in_report.dButtons.Ignition = is_key_pressed(KEY_I);
    gp->steel_battalion_in_report.dButtons.Start    = is_key_pressed(KEY_ENTER);

    gp->steel_battalion_in_report.dButtons.ToggleFiltControl    = is_key_pressed(KEY_1);
    gp->steel_battalion_in_report.dButtons.ToggleOxygenSupply    = is_key_pressed(KEY_2);
    gp->steel_battalion_in_report.dButtons.ToggleFuelFlowRate    = is_key_pressed(KEY_3);
    gp->steel_battalion_in_report.dButtons.ToggleBufferMaterial    = is_key_pressed(KEY_4);
    gp->steel_battalion_in_report.dButtons.ToggleVTLocation    = is_key_pressed(KEY_5);
    gp->steel_battalion_in_report.dButtons.Function1    = is_key_pressed(KEY_L);

    if ((local_modifiers & KEY_MOD_LSHIFT) && is_key_pressed(KEY_E)) {
        gp->steel_battalion_in_report.dButtons.CockpitHatch = true;
    }
    t++;
}

void core1_usb_host_entry() {
    sleep_ms(10);
    static pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = HOST_PIN_DP;
    tuh_configure(BOARD_HOST_RHPORT_NUM, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    tusb_rhport_init_t const host_init_config = {.role = TUSB_ROLE_HOST};
    tusb_init(BOARD_HOST_RHPORT_NUM, &host_init_config);

    uint32_t led_timer = 0;
    bool led_is_active = false;

    while (1) {
        tuh_task(); 
        uint32_t now = to_ms_since_boot(get_absolute_time());

        for (int i = 0; i < 3; i++) {
            if (device_activation_queue[i].needs_activation) {
                tuh_hid_receive_report(device_activation_queue[i].dev_addr, device_activation_queue[i].instance);
                gpio_put(PICO_LED_PIN, 1);
                led_timer = now;
                led_is_active = true;
                device_activation_queue[i].needs_activation = false;
            }
        }

        if (led_is_active && (now - led_timer >= 1000)) {
            gpio_put(PICO_LED_PIN, 0);
            led_is_active = false;
        }
    }
}

// Modern TinyUSB dynamic driver injection callback
/* void usbh_app_driver_get_cb(usbh_class_driver_t const** driver_t, uint8_t* count) 
{
    static usbh_class_driver_t const custom_driver = {
        #if CFG_TUSB_DEBUG >= 2
        .name       = "SBC",
        #endif
        .init       = sbch_init,
        .open       = sbch_open,
        .set_config = sbch_set_config,
        .xfer_cb    = sbch_xfer_cb,
        .close      = sbch_close
    };

    *driver_t = &custom_driver;
    *count = 1;
} */

// CHANGE THIS:
// void usbh_app_driver_get_cb(usbh_class_driver_t const** driver_t, uint8_t* count)

// TO THIS:
usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count)
{
    static usbh_class_driver_t const sbc_driver = {
        .init       = sbch_init,
        .open       = sbch_open,
        .set_config = sbch_set_config,
        .xfer_cb    = sbch_xfer_cb,
        .close      = sbch_close
    };

    *driver_count = 1;
    return &sbc_driver;
}
