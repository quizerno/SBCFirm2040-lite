#define CFG_TUH_ENABLED 1
#include "tusb.h"
#include "host/usbh_pvt.h" 

#include "class/sbc/sbc_host.h"
#include "device_profiles.h"
#include "tusb_gamepad.h"  
#include "input_mapping.h"
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

#include "hid_kbm.h"
#include "hid_gamepad.h"
#include "neopixel.h"
#include "usb_host_callbacks.h"

#define HOST_PIN_DP 0 
#define PICO_LED_PIN 25 
#define QUEUE_DEPTH 8
#define MOUSE_SENSITIVITY 45
#define fireButtonPin 15

int32_t accumulated_aim_x = 32767;
int32_t accumulated_aim_y = 32767;
int t = 0; 

queue_t gamepad_packet_queue;

// Feedback indicators
bool led_feedback_enable = true;
volatile sbc_leds_t cockpit_led_registry = {0};

/**
 * Safely extracts a 4-bit intensity value (0-15) from a raw packed byte array.
 * Eliminates individual structure member field checks.
 */
static inline uint8_t get_led_level_by_index(uint8_t const* raw_bytes, uint8_t led_index) {
    uint8_t byte_index = led_index / 2;
    if ((led_index % 2) == 0) {
        return raw_bytes[byte_index] & 0x0F;
    } else {
        return (raw_bytes[byte_index] >> 4) & 0x0F;
    }
}

// Forward Declarations
void apply_inputs_to_steel_battalion(Gamepad *gp, 
                                     generic_gamepad_data_t const* pad_data, 
                                     local_sbc_data_t const* sbc_data, 
                                     uint8_t input_source);
void test_led(void);									 
void core1_usb_host_entry(void);
void pack_steel_battalion_report(uint8_t *out_buf, Gamepad *gp);

// Structural mapping target configuration for internal layout workspaces
typedef struct __attribute__((packed)) {
    uint8_t led_data[20]; 
} xbox_led_payload_t;

void process_xbox_led_packet(uint8_t const* payload, uint16_t len) {
    if (len < 20) return; 
    sbc_leds_t temporary_led_map;
    memcpy(&temporary_led_map, payload, sizeof(sbc_leds_t));
    cockpit_led_registry = temporary_led_map;
}
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
    usb_packet_t pkt;

    while (1) {
        // =========================================================================
        // STEP 1: HOISTED RESET MATRIX (Runs first, at the top of every frame pass)
        // =========================================================================
        local_modifiers     = global_modifiers;
        for (int i = 0; i < 6; i++) local_keycodes[i] = global_keycodes[i];
        local_mouse_buttons = global_mouse_buttons;
        local_mouse_x       = global_mouse_x;
        local_mouse_y       = global_mouse_y;
        local_mouse_wheel   = global_mouse_wheel;

        global_mouse_x = 0; 
        global_mouse_y = 0; 
        global_mouse_wheel = 0;

        bool gamepad_activity = false;
        bool sbc_input        = false; 
        uint8_t current_input_source = 0; 
		
        generic_gamepad_data_t local_joy_data = {0};
        local_joy_data.hat = 8; 
        local_sbc_data_t local_sbc_data = {0};

        // =========================================================================
        // STEP 2: CROSS-CORE PAYLOAD RETRIEVAL & TARGET RECOGNITION (CORE 0 ROLE)
        // =========================================================================
        if (queue_try_remove(&gamepad_packet_queue, &pkt)) {

        // Direct Native/Emulated Steel Battalion Packet Tracking
        if (pkt.usage_id == 0x80) {
            // Clear the target translation register mapping space
            memset(&local_sbc_data, 0, sizeof(local_sbc_data_t));
            
            // Interpret pkt.report directly as the raw 26-byte hardware stream
            uint8_t const* rdata = (uint8_t const*)pkt.report;

            // =========================================================================
            // <<< FIXED MAPPING PARSER: Reconstruct fields matching your apply logic >>>
            // =========================================================================
            
            // 1. Reconstruct the 64-bit button field aligned exactly with apply bitmasks
            local_sbc_data.bButtons = 0;
            local_sbc_data.bButtons |= ((uint64_t)rdata[2]);       // wButtons[0] LSB -> bit 0
            local_sbc_data.bButtons |= ((uint64_t)rdata[3] << 8);  // wButtons[0] MSB -> bit 8
            local_sbc_data.bButtons |= ((uint64_t)rdata[4] << 16); // wButtons[1] LSB -> bit 16
            local_sbc_data.bButtons |= ((uint64_t)rdata[5] << 24); // wButtons[1] MSB -> bit 24
            
            // Reconstruct upper blocks cleanly, stripping the validation bit (0x80) safely
            local_sbc_data.bButtons |= ((uint64_t)(rdata[6] & 0x7F) << 32); // wButtons[2] LSB -> bit 32
            local_sbc_data.bButtons |= ((uint64_t)rdata[7] << 40); // wButtons[2] MSB -> bit 40

            // 2. Map single-byte analog components from their raw 26-byte packet offsets
            local_sbc_data.bAimingX       = rdata[9];   // Raw MSB alignment tracking
            local_sbc_data.bAimingY       = rdata[11];  // Raw MSB alignment tracking
            local_sbc_data.bRotationLever = rdata[13];  // Raw MSB alignment tracking
            local_sbc_data.bSightChangeX  = rdata[15];  // Raw MSB alignment tracking
            local_sbc_data.bSightChangeY  = rdata[17];  // Raw MSB alignment tracking

            // 3. Map single-byte pedal analog channels
            local_sbc_data.bLeftPedal     = rdata[19];  // Raw MSB alignment tracking
            local_sbc_data.bMiddlePedal   = rdata[21];  // Raw MSB alignment tracking
            local_sbc_data.bRightPedal    = rdata[23];  // Raw MSB alignment tracking

            // 4. Map final state knobs and levers
            local_sbc_data.bTunerDial     = rdata[24] & 0x0F; // Extract lower nibble safely
            local_sbc_data.bGearLever     = rdata[25];        // Direct shifter mapping index
			
            sbc_input = true;
            gamepad_activity = true;
            current_input_source = 2;  
        }
     
			
			
			
			
                     
            // Fallback Generic Controllers (PS4, D-Input, Joysticks)
            else if (pkt.usage_id == 0x04 || pkt.usage_id == 0x05 || pkt.usage_id == 0x99) {
                bool parsed_ok = route_and_parse_gamepad(pkt.report, pkt.len, pkt.dev_addr, &local_joy_data);
                if (parsed_ok) {
                    bool button_or_hat_active = (local_joy_data.buttons != 0) || (local_joy_data.hat != 8);
                    bool analog_sticks_active = (abs(local_joy_data.lx) > 15) || (abs(local_joy_data.ly) > 15);
                    
                    gamepad_activity = button_or_hat_active || analog_sticks_active;
                    if (gamepad_activity) current_input_source = 1;
                }
            }
        }


        // =========================================================================
        // STEP 3: EMULATION TRANSLATION AND PASSTHROUGH INTERPRETER
        // =========================================================================
        apply_inputs_to_steel_battalion(gp, &local_joy_data, &local_sbc_data, current_input_source);

        // =========================================================================
        // STEP 4: ACTIVITY MONITOR & HARDWARE SIGNALING GATES
        // =========================================================================
        bool key_active = false;
        for (int i = 0; i < 6; i++) { 
            if (local_keycodes[i] != 0) { 
                key_active = true; 
                break; 
            } 
        }
        bool mouse_active = (local_mouse_buttons != 0 || local_mouse_x != 0 || local_mouse_y != 0 || local_mouse_wheel != 0);
        bool gpio_active = (gpio_get(fireButtonPin) == 0);
		
        if (key_active || local_modifiers != 0 || mouse_active || gamepad_activity || gpio_active || sbc_input) {
            gpio_put(PICO_LED_PIN, 1);
        } else {
            gpio_put(PICO_LED_PIN, 0);
        }

        // =========================================================================
        // STEP 5: LED EXTRACTION STREAM HANDLING
        // =========================================================================
        if (led_feedback_enable) {
            sbc_leds_t current_led_frame = cockpit_led_registry;
            uint8_t cockpit_brightness_matrix[38]; 
            uint8_t const* raw_led_stream = (uint8_t const*)&current_led_frame;

            for (uint8_t i = 0; i < 38; i++) {
                uint8_t raw_level = get_led_level_by_index(raw_led_stream, i);
                cockpit_brightness_matrix[i] = raw_level;
            }
            (void)cockpit_brightness_matrix; // Warning gate bypass
        }

        // Keep internal translation layers active
        // Keep internal translation layers active
        tusb_gamepad_task();
        tud_task();

        // =========================================================================
        // <<< TRANSMISSION FIX: Package and push data to physical endpoints >>>
        // =========================================================================
        if (tud_ready() && tud_hid_ready()) {
            uint8_t outbound_console_buffer[26];
            
            if (current_input_source == 2) {
                // If the physical controller is connected, copy its raw 26 bytes directly
                // to eliminate layout discrepancies or calculation shift errors.
                memcpy(outbound_console_buffer, pkt.report, 26);
            } else {
                // Fallback: Run standard serialization mapper for mice/keyboards/generic pads
                pack_steel_battalion_report(outbound_console_buffer, gp);
            }
            
            // Broadcast the compiled hardware frame down endpoint 0
            tud_hid_report(0, outbound_console_buffer, 26);
        }
		
		
		
    }
    return 0;
}
void apply_inputs_to_steel_battalion(Gamepad *gp, 
                                     generic_gamepad_data_t const* pad_data, 
                                     local_sbc_data_t const* sbc_data, 
                                     uint8_t input_source) 
{
    memset(&gp->steel_battalion_in_report.dButtons, 0, sizeof(gp->steel_battalion_in_report.dButtons));
	
	
	// =========================================================================
    // BRANCH 1: NATIVE PASSTHROUGH MAP (Physical or Emulated Steel Battalion Controller Connected)
    // =========================================================================
    if (input_source == 2) {
        uint64_t b = sbc_data->bButtons;
		//bool passthrough_buttons = (b & 0xFF) > 0; 

		//gp->steel_battalion_in_report.dButtons.MainWeapon   = (b & (1ULL << 0)) || passthrough_buttons;
        gp->steel_battalion_in_report.gearLever     = sbc_data->bGearLever;		
        gp->steel_battalion_in_report.sightChangeX  = sbc_data->bSightChangeX;
        gp->steel_battalion_in_report.sightChangeY  = sbc_data->bSightChangeY;
        gp->steel_battalion_in_report.rotationLever = sbc_data->bRotationLever;
        
        gp->steel_battalion_in_report.dButtons.SightChange          = (b & (1ULL << 33)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleFiltControl    = (b & (1ULL << 34)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleOxygenSupply   = (b & (1ULL << 35)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleFuelFlowRate   = (b & (1ULL << 36)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleBufferMaterial = (b & (1ULL << 37)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleVTLocation     = (b & (1ULL << 38)) ? true : false;
		
        gp->steel_battalion_in_report.tunerDial     = sbc_data->bTunerDial;
        gp->steel_battalion_in_report.dButtons.Comm1 = (b & (1ULL << 28)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Comm2 = (b & (1ULL << 29)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Comm3 = (b & (1ULL << 30)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Comm4 = (b & (1ULL << 31)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Comm5 = (b & (1ULL << 32)) ? true : false;
		
        gp->steel_battalion_in_report.dButtons.Function1            = (b & (1ULL << 22)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Function2            = (b & (1ULL << 23)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Function3            = (b & (1ULL << 24)) ? true : false;	
        gp->steel_battalion_in_report.dButtons.ForecastShootingSystem = (b & (1ULL << 13)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Manipulator          = (b & (1ULL << 14)) ? true : false;
        gp->steel_battalion_in_report.dButtons.LineColorChange      = (b & (1ULL << 15)) ? true : false;
        gp->steel_battalion_in_report.dButtons.TankDetach          = (b & (1ULL << 19)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Override             = (b & (1ULL << 20)) ? true : false;
        gp->steel_battalion_in_report.dButtons.NightScope           = (b & (1ULL << 21)) ? true : false;
		
        gp->steel_battalion_in_report.dButtons.Washing              = (b & (1ULL << 16)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Extinguisher         = (b & (1ULL << 17)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Chaff                = (b & (1ULL << 18)) ? true : false;
        gp->steel_battalion_in_report.dButtons.WeaponConMain        = (b & (1ULL << 25)) ? true : false;
        gp->steel_battalion_in_report.dButtons.WeaponConSub         = (b & (1ULL << 26)) ? true : false;
        gp->steel_battalion_in_report.dButtons.WeaponConMagazine    = (b & (1ULL << 27)) ? true : false;

        gp->steel_battalion_in_report.aimingX       = sbc_data->bAimingX;
        gp->steel_battalion_in_report.aimingY       = sbc_data->bAimingY;
        
        gp->steel_battalion_in_report.dButtons.MainWeapon   = (b & (1ULL << 0)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Fire         = (b & (1ULL << 1)) ? true : false;
        gp->steel_battalion_in_report.dButtons.LockOn       = (b & (1ULL << 2)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Eject        = (b & (1ULL << 3)) ? true : false;
        gp->steel_battalion_in_report.dButtons.CockpitHatch = (b & (1ULL << 4)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Ignition     = (b & (1ULL << 5)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Start        = (b & (1ULL << 6)) ? true : false;
        gp->steel_battalion_in_report.dButtons.MultiMonitorOpenClose   = (b & (1ULL << 7)) ? true : false;
        gp->steel_battalion_in_report.dButtons.MultiMonitorMapZoomInOut = (b & (1ULL << 8)) ? true : false;
        gp->steel_battalion_in_report.dButtons.MultiMonitorModeSelect  = (b & (1ULL << 9)) ? true : false;
        gp->steel_battalion_in_report.dButtons.MultiMonitorSubMonitor  = (b & (1ULL << 10)) ? true : false;
        gp->steel_battalion_in_report.dButtons.MainMonitorZoomIn       = (b & (1ULL << 11)) ? true : false;
        gp->steel_battalion_in_report.dButtons.MainMonitorZoomOut      = (b & (1ULL << 12)) ? true : false;
		
        gp->steel_battalion_in_report.leftPedal     = sbc_data->bLeftPedal;
        gp->steel_battalion_in_report.middlePedal   = sbc_data->bMiddlePedal;
        gp->steel_battalion_in_report.rightPedal    = sbc_data->bRightPedal;

        // =========================================================================
        // <<< FIX: Explicitly sync the structure changes to the outbound buffer >>>
        // =========================================================================
        //gp->report_changed = true; 
        return;
    }
    // ... Rest of Emulation translation map remains identical
else{

    // =========================================================================
    // BRANCH 2: EMULATION TRANSLATION MAP (Keyboard, Mouse, Standard Gamepads, Custom HID Devices, or Flightsticks/HOTAS)
    // =========================================================================
    // D-Pad / Hat Switch Cam Parsing
	
	//TO DO: Create input source identifier for gamepad and separate it from mouse and keyboard
	
    if (pad_data->hat == 0) gp->steel_battalion_in_report.sightChangeY = -8000;
    if (pad_data->hat == 4) gp->steel_battalion_in_report.sightChangeY = 8000;
    
    if (is_key_pressed(KEY_A))      gp->steel_battalion_in_report.rotationLever = -32768; 
    else if (is_key_pressed(KEY_D)) gp->steel_battalion_in_report.rotationLever = 32767;  
    else                            gp->steel_battalion_in_report.rotationLever = 0;      


    if (input_source == 1) { 
        if (pad_data->buttons & (1 << 0)) gp->steel_battalion_in_report.dButtons.MainWeapon = true; 
        if (pad_data->buttons & (1 << 1)) gp->steel_battalion_in_report.dButtons.Fire       = true; 
        if (pad_data->buttons & (1 << 3)) gp->steel_battalion_in_report.dButtons.LockOn     = true; 
    } else {
        gp->steel_battalion_in_report.dButtons.MainWeapon = is_mouse_pressed(MOUSE_BUTTON_LEFT);
        gp->steel_battalion_in_report.dButtons.Fire       = is_mouse_pressed(MOUSE_BUTTON_RIGHT);
		    accumulated_aim_x += (local_mouse_x * MOUSE_SENSITIVITY);
    accumulated_aim_y += (local_mouse_y * MOUSE_SENSITIVITY);

    if (accumulated_aim_x > 65535) accumulated_aim_x = 65535;
    if (accumulated_aim_x < 0)     accumulated_aim_x = 0;
    if (accumulated_aim_y > 65535) accumulated_aim_y = 65535;
    if (accumulated_aim_y < 0)     accumulated_aim_y = 0;
	
	
	
	
		
		gp->steel_battalion_in_report.aimingX       = (uint16_t)accumulated_aim_x; 
        gp->steel_battalion_in_report.aimingY 		= (uint16_t)accumulated_aim_y; 
		
		
    }
    
    gp->steel_battalion_in_report.dButtons.CockpitHatch = is_key_pressed(KEY_P);
    gp->steel_battalion_in_report.dButtons.Eject        = is_key_pressed(KEY_SPACE);
    gp->steel_battalion_in_report.dButtons.Ignition     = is_key_pressed(KEY_I);
    gp->steel_battalion_in_report.dButtons.Start        = is_key_pressed(KEY_ENTER);


    // =========================================================================
    // BRANCH 3: GPIO Readings (Direct Input to Adapter)
    // =========================================================================
	
    if (gpio_get(fireButtonPin) == 0) {
        gp->steel_battalion_in_report.dButtons.MainWeapon = true;
    }
}
	
}
void core1_usb_host_entry(void) {
    sleep_ms(10);
    static pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = HOST_PIN_DP;
    tuh_configure(BOARD_HOST_RHPORT_NUM, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    tusb_rhport_init_t const host_init_config = {.role = TUSB_ROLE_HOST};
    tusb_init(BOARD_HOST_RHPORT_NUM, &host_init_config);

    while (1) {
        tuh_task(); 
        if (physical_sbc_connected && physical_sbc_addr != 0) {
            tuh_sbc_receive_report(physical_sbc_addr, physical_sbc_instance);
        }
        
        for (int i = 0; i < 3; i++) {
            if (device_activation_queue[i].needs_activation) {
                tuh_hid_receive_report(device_activation_queue[i].dev_addr, device_activation_queue[i].instance);
                device_activation_queue[i].needs_activation = false; 
            }
        }
    }
}

void test_led(void) {
    for (int i = 0; i < 3; i++) {
        gpio_put(PICO_LED_PIN, 1); sleep_ms(40);
        gpio_put(PICO_LED_PIN, 0); sleep_ms(40);
    }
}

void pack_steel_battalion_report(uint8_t *out_buf, Gamepad *gp) {
    // Clear out transmission container and assign tracking headers
    memset(out_buf, 0, 26);
    out_buf[0] = 0x00; // Report ID Sequence
    out_buf[1] = 0x1A; // Real XID Packet Descriptor Length Bounds (26 Bytes)

    // Reconstruct the 3 button words byte-by-byte from the unpacked dButtons struct
    uint16_t w0 = 0, w1 = 0, w2 = 0;

    // Word 0 Mapping
    if (gp->steel_battalion_in_report.dButtons.MainWeapon)   w0 |= (1 << 0);
    if (gp->steel_battalion_in_report.dButtons.Fire)         w0 |= (1 << 1);
    if (gp->steel_battalion_in_report.dButtons.LockOn)       w0 |= (1 << 2);
    if (gp->steel_battalion_in_report.dButtons.Eject)        w0 |= (1 << 3);
    if (gp->steel_battalion_in_report.dButtons.CockpitHatch) w0 |= (1 << 4);
    if (gp->steel_battalion_in_report.dButtons.Ignition)     w0 |= (1 << 5);
    if (gp->steel_battalion_in_report.dButtons.Start)        w0 |= (1 << 6);
    if (gp->steel_battalion_in_report.dButtons.MultiMonitorOpenClose)   w0 |= (1 << 7);
    if (gp->steel_battalion_in_report.dButtons.MultiMonitorMapZoomInOut) w0 |= (1 << 8);
    if (gp->steel_battalion_in_report.dButtons.MultiMonitorModeSelect)  w0 |= (1 << 9);
    if (gp->steel_battalion_in_report.dButtons.MultiMonitorSubMonitor)  w0 |= (1 << 10);
    if (gp->steel_battalion_in_report.dButtons.MainMonitorZoomIn)       w0 |= (1 << 11);
    if (gp->steel_battalion_in_report.dButtons.MainMonitorZoomOut)      w0 |= (1 << 12);
    if (gp->steel_battalion_in_report.dButtons.ForecastShootingSystem)  w0 |= (1 << 13);
    if (gp->steel_battalion_in_report.dButtons.Manipulator)          w0 |= (1 << 14);
    if (gp->steel_battalion_in_report.dButtons.LineColorChange)      w0 |= (1 << 15);

    // Word 1 Mapping
    if (gp->steel_battalion_in_report.dButtons.Washing)           w1 |= (1 << 0);
    if (gp->steel_battalion_in_report.dButtons.Extinguisher)      w1 |= (1 << 1);
    if (gp->steel_battalion_in_report.dButtons.Chaff)             w1 |= (1 << 2);
    if (gp->steel_battalion_in_report.dButtons.TankDetach)        w1 |= (1 << 3);
    if (gp->steel_battalion_in_report.dButtons.Override)          w1 |= (1 << 4);
    if (gp->steel_battalion_in_report.dButtons.NightScope)        w1 |= (1 << 5);
    if (gp->steel_battalion_in_report.dButtons.Function1)         w1 |= (1 << 6);
    if (gp->steel_battalion_in_report.dButtons.Function2)         w1 |= (1 << 7);
    if (gp->steel_battalion_in_report.dButtons.Function3)         w1 |= (1 << 8);
    if (gp->steel_battalion_in_report.dButtons.WeaponConMain)     w1 |= (1 << 9);
    if (gp->steel_battalion_in_report.dButtons.WeaponConSub)      w1 |= (1 << 10);
    if (gp->steel_battalion_in_report.dButtons.WeaponConMagazine) w1 |= (1 << 11);
    if (gp->steel_battalion_in_report.dButtons.Comm1)             w1 |= (1 << 12);
    if (gp->steel_battalion_in_report.dButtons.Comm2)             w1 |= (1 << 13);
    if (gp->steel_battalion_in_report.dButtons.Comm3)             w1 |= (1 << 14);
    if (gp->steel_battalion_in_report.dButtons.Comm4)             w1 |= (1 << 15);

    // Word 2 Mapping
    if (gp->steel_battalion_in_report.dButtons.Comm5)             w2 |= (1 << 0);
    if (gp->steel_battalion_in_report.dButtons.SightChange)       w2 |= (1 << 1);
    if (gp->steel_battalion_in_report.dButtons.ToggleFiltControl) w2 |= (1 << 2);
    if (gp->steel_battalion_in_report.dButtons.ToggleOxygenSupply) w2 |= (1 << 3);
    if (gp->steel_battalion_in_report.dButtons.ToggleFuelFlowRate) w2 |= (1 << 4);
    if (gp->steel_battalion_in_report.dButtons.ToggleBufferMaterial) w2 |= (1 << 5);
    if (gp->steel_battalion_in_report.dButtons.ToggleVTLocation)  w2 |= (1 << 6);

    // Enforce validation bit
    w2 |= 0x80; 

    // Write button words into the transmission payload matrix
    out_buf[2] = (uint8_t)(w0 & 0xFF);
    out_buf[3] = (uint8_t)(w0 >> 8);
    out_buf[4] = (uint8_t)(w1 & 0xFF);
    out_buf[5] = (uint8_t)(w1 >> 8);
    out_buf[6] = (uint8_t)(w2 & 0xFF);
    out_buf[7] = (uint8_t)(w2 >> 8);

    // Format 8-bit analog metrics back to 16-bit physical words
    uint16_t ax = (uint16_t)gp->steel_battalion_in_report.aimingX << 8;
    uint16_t ay = (uint16_t)gp->steel_battalion_in_report.aimingY << 8;
    uint16_t rl = (uint16_t)gp->steel_battalion_in_report.rotationLever << 8;
    uint16_t sx = (uint16_t)gp->steel_battalion_in_report.sightChangeX << 8;
    uint16_t sy = (uint16_t)gp->steel_battalion_in_report.sightChangeY << 8;
    uint16_t lp = (uint16_t)gp->steel_battalion_in_report.leftPedal << 8;
    uint16_t mp = (uint16_t)gp->steel_battalion_in_report.middlePedal << 8;
    uint16_t rp = (uint16_t)gp->steel_battalion_in_report.rightPedal << 8;

    out_buf[8]  = (uint8_t)(ax & 0xFF);  out_buf[9]  = (uint8_t)(ax >> 8);
    out_buf[10] = (uint8_t)(ay & 0xFF);  out_buf[11] = (uint8_t)(ay >> 8);
    out_buf[12] = (uint8_t)(rl & 0xFF);  out_buf[13] = (uint8_t)(rl >> 8);
    out_buf[14] = (uint8_t)(sx & 0xFF);  out_buf[15] = (uint8_t)(sx >> 8);
    out_buf[16] = (uint8_t)(sy & 0xFF);  out_buf[17] = (uint8_t)(sy >> 8);
    out_buf[18] = (uint8_t)(lp & 0xFF);  out_buf[19] = (uint8_t)(lp >> 8);
    out_buf[20] = (uint8_t)(mp & 0xFF);  out_buf[21] = (uint8_t)(mp >> 8);
    out_buf[22] = (uint8_t)(rp & 0xFF);  out_buf[23] = (uint8_t)(rp >> 8);

    out_buf[24] = gp->steel_battalion_in_report.tunerDial & 0x0F;
    out_buf[25] = gp->steel_battalion_in_report.gearLever;
}


// Custom app driver hook mapping table
usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count)
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

    *driver_count = 1;
    return &custom_driver;
}


// Force this array structure into your Core 0 device descriptor configuration:
const uint8_t sbc_xid_descriptor[] = {
    0x10, 0x42, 0x00, 0x01, 0x01, 0x02, 0x00, 0x06, 
    0x5E, 0x04, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00
};


bool tud_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) 
{
    (void) rhport;
    if (stage != CONTROL_STAGE_SETUP) return true;

    // Intercept Original Xbox XID Vendor Requests
    if (request->bmRequestType == 0xC1 && request->bRequest == 0x01) 
    {
        if (request->wValue == 0x0100) 
        { 
            // Inbound Capabilities Request
            static const uint8_t sbc_caps[] = { 0x06, 0x01, 0x00, 0x02, 0x14, 0x00 };
            tud_control_xfer(rhport, request, (void*)(uintptr_t)sbc_caps, sizeof(sbc_caps));
            return true;
        }
        else if (request->wValue == 0x0200) 
        { 
            // Inbound Extended Configuration Stream
            static const uint8_t sbc_ext_cfg[] = { 0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0x80, 0x14 };
            tud_control_xfer(rhport, request, (void*)(uintptr_t)sbc_ext_cfg, sizeof(sbc_ext_cfg));
            return true;
        }
    }
    
    return true; 
}