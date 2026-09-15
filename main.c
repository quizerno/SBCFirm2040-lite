
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
void apply_inputs_to_steel_battalion(Gamepad *gp, 
                                     generic_gamepad_data_t const* pad_data, 
                                     local_sbc_data_t const* sbc_data, 
                                     uint8_t input_source);


void core1_usb_host_entry();

//To do
//Create a system where multiple HID devices do not have overlap (if both are defined in device profiles, the buttons start adding sequentially)
//Experiment with creating a separate function for the passthrough system.
//See how pass through data is stored
//Test custome HID Pedals
//See if multiple endpoints can be parsed


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

    // Instantiate separate tracking blocks at the top of the main loop iteration
    generic_gamepad_data_t local_joy_data = {0};
    local_joy_data.hat = 8; 

    local_sbc_data_t local_sbc_data = {0}; // <-- NEW DEDICATED STRUCT

    uint8_t current_input_source = 0; // 0 = KBM/Idle, 1 = Generic Gamepad, 2 = Native SBC Hardware
    
    usb_packet_t pkt;
    if (queue_try_remove(&gamepad_packet_queue, &pkt)) {
      if (pkt.usage_id == 0x80) {
            sbch_interface_t *xid_itf = (sbch_interface_t *)pkt.report;
            
            // Clean direct copy bypassing strict type casting guardrails
            memcpy(&local_sbc_data, &xid_itf->pad, sizeof(local_sbc_data_t)); 
            
            gamepad_activity = true;
            current_input_source = 2; // Tag source as physical hardware
        } 
        else {
            // Process standard gamepads normally using the generic struct path
            bool parsed_ok = route_and_parse_gamepad(pkt.report, pkt.len, pkt.dev_addr, &local_joy_data);
            if (parsed_ok) {
                bool button_or_hat_active = (local_joy_data.buttons != 0) || (local_joy_data.hat != 8);
                bool analog_sticks_active = (abs(local_joy_data.lx) > 15) || (abs(local_joy_data.ly) > 15);
                
                gamepad_activity = button_or_hat_active || analog_sticks_active;
                if (gamepad_activity) current_input_source = 1;
            }
        }
    }

    // Pass BOTH structures cleanly into the updated interpreter block
    apply_inputs_to_steel_battalion(gp, &local_joy_data, &local_sbc_data, current_input_source);




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

// Update function prototype to accept input source layout tags
void apply_inputs_to_steel_battalion(Gamepad *gp, 
                                     generic_gamepad_data_t const* pad_data, 
                                     local_sbc_data_t const* sbc_data, 
                                     uint8_t input_source) 
{
    // Clear out button flags at the start of each calculation frame pass
    memset(&gp->steel_battalion_in_report.dButtons, 0, sizeof(gp->steel_battalion_in_report.dButtons));
	
    // =========================================================================
    // BRANCH 1: NATIVE PASSTHROUGH MAP (Physical Steel Battalion Controller Connected)
    // =========================================================================
    if (input_source == 2) {
        uint64_t b = sbc_data->bButtons;

        // 1. Map standard boolean push-buttons cleanly using the exact native names
        gp->steel_battalion_in_report.dButtons.MainWeapon   = (b & (1ULL << 0))  ? true : false;
        gp->steel_battalion_in_report.dButtons.Fire         = (b & (1ULL << 1))  ? true : false;
        gp->steel_battalion_in_report.dButtons.LockOn       = (b & (1ULL << 2))  ? true : false;
        gp->steel_battalion_in_report.dButtons.Eject        = (b & (1ULL << 3))  ? true : false;
        gp->steel_battalion_in_report.dButtons.CockpitHatch = (b & (1ULL << 4))  ? true : false;
        gp->steel_battalion_in_report.dButtons.Ignition     = (b & (1ULL << 5))  ? true : false;
        gp->steel_battalion_in_report.dButtons.Start        = (b & (1ULL << 6))  ? true : false;
        
        // 2. Map auxiliary dashboard toggle switches 
        gp->steel_battalion_in_report.dButtons.ToggleFiltControl   = (b & (1ULL << 14)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleOxygenSupply  = (b & (1ULL << 15)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleFuelFlowRate  = (b & (1ULL << 16)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleBufferMaterial= (b & (1ULL << 17)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleVTLocation    = (b & (1ULL << 18)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Function1           = (b & (1ULL << 19)) ? true : false;

        // 3. Passthrough the analog telemetry variables using matching data naming schemes
        gp->steel_battalion_in_report.aimingX       = sbc_data->bAimingX;
        gp->steel_battalion_in_report.aimingY       = sbc_data->bAimingY;
        gp->steel_battalion_in_report.sightChangeX  = sbc_data->bSightChangeX;
        gp->steel_battalion_in_report.sightChangeY  = sbc_data->bSightChangeY;
        gp->steel_battalion_in_report.rotationLever = sbc_data->bRotationLever;
        
        // 4. Map the discrete items using the proper struct variables
        gp->steel_battalion_in_report.gearLever     = sbc_data->bGearLever;
        
        // (If your output report exposes explicit properties for pedals/dials, map them here):
        // gp->steel_battalion_in_report.leftPedal  = sbc_data->bLeftPedal;
        // gp->steel_battalion_in_report.tunerDial  = sbc_data->bTunerDial;

        return; // Exit out immediately
    }

    // =========================================================================
    // BRANCH 2: EMULATION TRANSLATION MAP (Keyboard, Mouse, Standard Gamepads, Custom HID Devices, or Flightsticks/HOTAS)
    // =========================================================================
    // (Your existing mapping rules remain active below for secondary devices)
    if (pad_data->hat == 0)   gp->steel_battalion_in_report.sightChangeY = -8000;
    if (pad_data->hat == 4)   gp->steel_battalion_in_report.sightChangeY = 8000;
    
    if (is_key_pressed(KEY_A)) gp->steel_battalion_in_report.rotationLever = -32768; 
    else if (is_key_pressed(KEY_D)) gp->steel_battalion_in_report.rotationLever = 32767;  
    else gp->steel_battalion_in_report.rotationLever = 0;      

    // ... rest of your standard keyboard/mouse/gamepad logic continues normally here ...
    if (input_source == 1) { // Generic Gamepad Translation
        if (pad_data->buttons & (1 << 0)) gp->steel_battalion_in_report.dButtons.MainWeapon = true; 
        if (pad_data->buttons & (1 << 1)) gp->steel_battalion_in_report.dButtons.Fire       = true; 
        if (pad_data->buttons & (1 << 3)) gp->steel_battalion_in_report.dButtons.LockOn     = true; 
    } else {
        // Keyboard & Mouse Emulation
        gp->steel_battalion_in_report.dButtons.MainWeapon = is_mouse_pressed(MOUSE_BUTTON_LEFT);
        gp->steel_battalion_in_report.dButtons.Fire       = is_mouse_pressed(MOUSE_BUTTON_RIGHT);
    }
    
    // Core buttons for emulation tracking
    gp->steel_battalion_in_report.dButtons.CockpitHatch = is_key_pressed(KEY_P);
    gp->steel_battalion_in_report.dButtons.Eject        = is_key_pressed(KEY_SPACE);
    gp->steel_battalion_in_report.dButtons.Ignition     = is_key_pressed(KEY_I);
    gp->steel_battalion_in_report.dButtons.Start        = is_key_pressed(KEY_ENTER);
	
	
	
	
	    // =========================================================================
    // BRANCH 3: GPIO Readings (Direct Input to Adapter)
    // =========================================================================
    // (Your existing mapping rules remain active below for secondary devices)
	
	if(gpio_get(fireButtonPin)==0){
	gp->steel_battalion_in_report.dButtons.MainWeapon = true;
	}
	
	
	
	
	
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
