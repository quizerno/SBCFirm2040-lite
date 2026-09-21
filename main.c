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




//feedback
bool led_feedback_enable = true;
volatile sbc_leds_t cockpit_led_registry = {0};

/**
 * Safely extracts a 4-bit intensity value (0-15) from a raw packed byte array.
 * Eliminates individual structure member field checks.
 */
static inline uint8_t get_led_level_by_index(uint8_t const* raw_bytes, uint8_t led_index) {
    // Each byte contains two indicators (2 channels * 4 bits = 8 bits)
    uint8_t byte_index = led_index / 2;
    
    if ((led_index % 2) == 0) {
        // Even indexes sit in the lower 4 bits (lower nibble)
        return raw_bytes[byte_index] & 0x0F;
    } else {
        // Odd indexes sit in the upper 4 bits (upper nibble)
        return (raw_bytes[byte_index] >> 4) & 0x0F;
    }
}


void apply_inputs_to_steel_battalion(Gamepad *gp, 
                                     generic_gamepad_data_t const* pad_data, 
                                     local_sbc_data_t const* sbc_data, 
                                     uint8_t input_source);
									 
void test_led();									 
void core1_usb_host_entry();
void pack_steel_battalion_report(uint8_t *out_buf, Gamepad *gp);

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
        uint8_t current_input_source = 0; // 0 = KBM/Idle, 1 = Generic Gamepad, 2 = Native SBC Hardware
		
        generic_gamepad_data_t local_joy_data = {0};
        local_joy_data.hat = 8; 
        local_sbc_data_t local_sbc_data = {0};

             // =========================================================================
        // STEP 2: CROSS-CORE PAYLOAD RETRIEVAL & TARGET RECOGNITION (CORE 0 ROLE)
        // =========================================================================
        if (queue_try_remove(&gamepad_packet_queue, &pkt)) {

            // Direct Native/Emulated Steel Battalion Packet Tracking
            if (pkt.usage_id == 0x80) {
                             memset(&local_sbc_data, 0, sizeof(local_sbc_data_t));
                
                // Cast the inter-core buffer directly back to the native gamepad struct
                sbc_gamepad_t const* incoming_pad = (sbc_gamepad_t const*)pkt.report;
                
                // FIXED LAYOUT STRUCT PASS: Map fields cleanly to our tracking registers
				local_sbc_data.bButtons       = ((uint64_t)incoming_pad->wButtons[2] << 32) |
												((uint64_t)incoming_pad->wButtons[1] << 16) |
												(uint64_t)incoming_pad->wButtons[0];
                // Downscale the 16-bit high-resolution inputs to 8-bit tracking spaces safely
                local_sbc_data.bAimingX       = (uint8_t)(incoming_pad->bAimingX >> 8);
                local_sbc_data.bAimingY       = (uint8_t)(incoming_pad->bAimingY >> 8);
                local_sbc_data.bRotationLever = (uint8_t)(incoming_pad->bRotationLever >> 8);
                local_sbc_data.bSightChangeX  = (uint8_t)(incoming_pad->bSightChangeX >> 8);
                local_sbc_data.bSightChangeY  = (uint8_t)(incoming_pad->bSightChangeY >> 8);
                
                // Extract linear foot pedal pressures
                local_sbc_data.bLeftPedal     = (uint8_t)(incoming_pad->bLeftPedal >> 8);
                local_sbc_data.bMiddlePedal   = (uint8_t)(incoming_pad->bMiddlePedal >> 8);
                local_sbc_data.bRightPedal    = (uint8_t)(incoming_pad->bRightPedal >> 8);
                
                // Keep discrete step dial locations clean
                local_sbc_data.bTunerDial     = incoming_pad->bTunerDial;
                local_sbc_data.bGearLever     = incoming_pad->bGearLever;
                
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
        // STEP 5: Steel Battalion LED Feedback (From Xbox or Emulator)
        // =========================================================================


if (physical_sbc_connected && physical_sbc_addr != 0) {
    
    // Snapshot the current live LED configurations sent from the Xbox console
    sbc_leds_t output_mirror_frame = cockpit_led_registry;
    
    // Push the struct straight to the original controller over USB Host!
    // This executes on Core 0 but passes a non-blocking request to the USB transfer queue.
    tuh_sbc_set_leds(physical_sbc_addr, physical_sbc_instance, &output_mirror_frame);
}

if(led_feedback_enable){
		sbc_leds_t current_led_frame = cockpit_led_registry;
		
		// Flat scratchpad array tracking independent illumination variables (0-15)
// Array indices correspond exactly to the layout order defined in sbc_leds_t
uint8_t cockpit_brightness_matrix[38]; 

// Create a safe, snapshot buffer of our incoming raw data
// We cast our volatile registry directly back to its raw byte representation
uint8_t const* raw_led_stream = (uint8_t const*)&cockpit_led_registry;

// Loop through all 38 valid peripheral indicators in a single swift pass!
for (uint8_t i = 0; i < 38; i++) {
    // Extract the raw intensity token (0 to 15) without a single IF statement
    uint8_t raw_level = get_led_level_by_index(raw_led_stream, i);
    
    // Store it directly into your tracking table for active debug inspection
    cockpit_brightness_matrix[i] = raw_level;
    
    // FUTURE-PROOF HARDWARE COUPLING POINT:
    // When you decide on a hardware mechanism, you can drop a single line here:
    // e.g., set_pwm_duty_cycle(i, raw_level * 17); // For direct GPIO/PWM
    // e.g., tx_shift_buffer[i] = raw_level;        // For serial shift register arrays
}//end of for
}//end led_feedback_enable


/*         tusb_gamepad_task();
        tud_task(); 
		 */
		
		
		
		        if (tud_hid_ready()) 
        {
            uint8_t downstream_tx_frame[26];
            
            // Unpack your current Gamepad structures directly into the 26-byte frame buffer
            pack_steel_battalion_report(downstream_tx_frame, gp);
            
            // Fire the completed payload package down endpoint 0/1 to the physical console line
            tud_hid_report(0, downstream_tx_frame, 26);
        }

        // Keep internal TinyUSB processing cycles moving forward
        tusb_gamepad_task();
        tud_task(); 
		
		
		
    }
    return 0;
}
void apply_inputs_to_steel_battalion(Gamepad *gp, 
                                     generic_gamepad_data_t const* pad_data, 
                                     local_sbc_data_t const* sbc_data, 
                                     uint8_t input_source) 
{
    // Clear out button flags at the start of each calculation frame pass
    memset(&gp->steel_battalion_in_report.dButtons, 0, sizeof(gp->steel_battalion_in_report.dButtons));
	
    // =========================================================================
    // BRANCH 1: NATIVE PASSTHROUGH MAP (Direct Structure Map Mode - FIXED DEDUPLICATION)
    // =========================================================================
    if (input_source == 2) {
        uint64_t b = sbc_data->bButtons;

        // Left Block Analog Core 
        gp->steel_battalion_in_report.gearLever     = sbc_data->bGearLever;		
        gp->steel_battalion_in_report.sightChangeX  = sbc_data->bSightChangeX;
        gp->steel_battalion_in_report.sightChangeY  = sbc_data->bSightChangeY;
        gp->steel_battalion_in_report.rotationLever = sbc_data->bRotationLever;
        
        // Bit-mask maps
        gp->steel_battalion_in_report.dButtons.SightChange          = (b & (1ULL << 33)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleFiltControl    = (b & (1ULL << 34)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleOxygenSupply   = (b & (1ULL << 35)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleFuelFlowRate   = (b & (1ULL << 36)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleBufferMaterial = (b & (1ULL << 37)) ? true : false;
        gp->steel_battalion_in_report.dButtons.ToggleVTLocation     = (b & (1ULL << 38)) ? true : false;
		
        // Middle Block
        gp->steel_battalion_in_report.tunerDial     = sbc_data->bTunerDial;
        gp->steel_battalion_in_report.dButtons.Comm1 = (b & (1ULL << 28)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Comm2 = (b & (1ULL << 29)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Comm3 = (b & (1ULL << 30)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Comm4 = (b & (1ULL << 31)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Comm5 = (b & (1ULL << 32)) ? true : false;
		
        // Center-Right Panel
        gp->steel_battalion_in_report.dButtons.Function1            = (b & (1ULL << 22)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Function2            = (b & (1ULL << 23)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Function3            = (b & (1ULL << 24)) ? true : false;	
        gp->steel_battalion_in_report.dButtons.ForecastShootingSystem = (b & (1ULL << 13)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Manipulator          = (b & (1ULL << 14)) ? true : false;
        gp->steel_battalion_in_report.dButtons.LineColorChange      = (b & (1ULL << 15)) ? true : false;
        gp->steel_battalion_in_report.dButtons.TankDetach          = (b & (1ULL << 19)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Override             = (b & (1ULL << 20)) ? true : false;
        gp->steel_battalion_in_report.dButtons.NightScope           = (b & (1ULL << 21)) ? true : false;
		
        // Weapon Switches
        gp->steel_battalion_in_report.dButtons.Washing              = (b & (1ULL << 16)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Extinguisher         = (b & (1ULL << 17)) ? true : false;
        gp->steel_battalion_in_report.dButtons.Chaff                = (b & (1ULL << 18)) ? true : false;
        gp->steel_battalion_in_report.dButtons.WeaponConMain        = (b & (1ULL << 25)) ? true : false;
        gp->steel_battalion_in_report.dButtons.WeaponConSub         = (b & (1ULL << 26)) ? true : false;
        gp->steel_battalion_in_report.dButtons.WeaponConMagazine    = (b & (1ULL << 27)) ? true : false;

        // Stick Grip Switches & Right Block Analog
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
		
        // Pedal Block
        gp->steel_battalion_in_report.leftPedal     = sbc_data->bLeftPedal;
        gp->steel_battalion_in_report.middlePedal   = sbc_data->bMiddlePedal;
        gp->steel_battalion_in_report.rightPedal    = sbc_data->bRightPedal;

        return;
    }

    // =========================================================================
    // BRANCH 2: EMULATION TRANSLATION MAP (Generic HID Fallback Peripherals)
    // =========================================================================
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
    }
    
    gp->steel_battalion_in_report.dButtons.CockpitHatch = is_key_pressed(KEY_P);
    gp->steel_battalion_in_report.dButtons.Eject        = is_key_pressed(KEY_SPACE);
    gp->steel_battalion_in_report.dButtons.Ignition     = is_key_pressed(KEY_I);
    gp->steel_battalion_in_report.dButtons.Start        = is_key_pressed(KEY_ENTER);

    if (gpio_get(fireButtonPin) == 0) {
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

    while (1) {
        tuh_task(); 
        
        // =========================================================================
        // NATIVE XID HOT-PLUG RAMP PUMP
        // =========================================================================
        // If a real Steel Battalion controller is attached, keep its host pipe pumped
        if (physical_sbc_connected && physical_sbc_addr != 0) {
            tuh_sbc_receive_report(physical_sbc_addr, physical_sbc_instance);
        }
        
        // Polling scheduler loop to ensure standard gamepads/peripherals wake up safely
        for (int i = 0; i < 3; i++) {
            if (device_activation_queue[i].needs_activation) {
                tuh_hid_receive_report(device_activation_queue[i].dev_addr, device_activation_queue[i].instance);
                device_activation_queue[i].needs_activation = false; 
            }
        }
    }
}
// Replace your old usbh_app_driver_get_cb block with this exact signature layout:
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

//working function
/* bool tud_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) 
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
} */


// Struct defining the target configuration mapping for your internal workspace
typedef struct __attribute__((packed)) {
    uint8_t led_data[20]; // 20 bytes containing the 40-bit LED brightness clusters
} xbox_led_payload_t;


/**
 * Extracts raw host output configurations and maps them to target hardware registers.
 * Mimics command mirroring by slicing target bytes out of the packet envelope.
 */
void process_xbox_led_packet(uint8_t const* payload, uint16_t len)
{
    // The Original Xbox out packet normally provides standard byte counts matching our profile.
    // Defensively bound check the packet stream.
    if (len < 20) return; 

    // Mirroring execution structure: Cast slice parameters cleanly into active states
    // Byte indices offset to align with the physical 4-bit nibble indicators
    sbc_leds_t temporary_led_map;
    memcpy(&temporary_led_map, payload, sizeof(sbc_leds_t));

    // Atomically transfer data variables to your system registry
    // This allows Core 1 or your physical display routines to safely sample state changes
    cockpit_led_registry = temporary_led_map;

    // Optional Diagnostic Flash Loop hook
    // test_led(); // Trigger confirmation cycles over your diagnostic framework pins
}

// Add/Edit this block inside main.c
bool tud_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) 
{
    (void) rhport;
    if (stage != CONTROL_STAGE_SETUP) return true;

    // 1. Intercept Original Xbox XID Vendor Requests (Capabilities / Config)
    if (request->bmRequestType == 0xC1 && request->bRequest == 0x01) 
    {
        if (request->wValue == 0x0100) 
        { 
            static const uint8_t sbc_caps[] = { 0x06, 0x01, 0x00, 0x02, 0x14, 0x00 };
            tud_control_xfer(rhport, request, (void*)(uintptr_t)sbc_caps, sizeof(sbc_caps));
            return true;
        }
        else if (request->wValue == 0x0200) 
        { 
            static const uint8_t sbc_ext_cfg[] = { 0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0x80, 0x14 };
            tud_control_xfer(rhport, request, (void*)(uintptr_t)sbc_ext_cfg, sizeof(sbc_ext_cfg));
            return true;
        }
    }
    
    // =========================================================================
    // NEW CORRECTION: INTERCEPT ASYNCHRONOUS HOST LED OUT_REPORTS
    // =========================================================================
    // bmRequestType: 0x21 (Class, Interface, Recipient) | bRequest: 0x09 (SET_REPORT)
    // wValue High Byte 0x02 indicates an Output Report payload envelope target
    if (request->bmRequestType == 0x21 && request->bRequest == 0x09 && (request->wValue >> 8) == 0x02)
    {
        // Allocate a safe scratch-buffer tracking window
        static xbox_led_payload_t inbound_led_buffer;
        
        uint16_t expected_len = request->wLength;
        if (expected_len > sizeof(inbound_led_buffer)) {
            expected_len = sizeof(inbound_led_buffer);
        }
        
        // Prepare the control endpoint setup stack to read the data segment directly
        tud_control_xfer(rhport, request, &inbound_led_buffer, expected_len);
        
        // MIRROR TRANSFERS OPERATION: Pipe the extracted pieces across to our processing block.
        // We call an external parsing utility to map these bytes into your local tracking state.
        process_xbox_led_packet(inbound_led_buffer.led_data, expected_len);
        
        return true;
    }
    
    return true; 
}






void test_led(){
    for (int i = 0; i < 3; i++) {
        gpio_put(PICO_LED_PIN, 1); sleep_ms(40);
        gpio_put(PICO_LED_PIN, 0); sleep_ms(40);
    }
}


// =========================================================================
// PACKING UTILITY: TRANSFORMS INTERNAL GAMEPAD DATA INTO STANDARD XID OUT BLOCK
// =========================================================================
void pack_steel_battalion_report(uint8_t *out_buf, Gamepad *gp) 
{
    // Clear out the 26-byte frame buffer entirely
    memset(out_buf, 0, 26);
    
    // Frame metadata/Report IDs typical for original Xbox controllers
    out_buf[0] = 0x00; 
    out_buf[1] = 0x14; // Specifies the standard payload length tracking byte (20 bytes data)

    // Reconstruct the 40-bit (5 byte) digital button array from the mapping
    uint64_t b = 0;
    if (gp->steel_battalion_in_report.dButtons.MainWeapon)    b |= (1ULL << 0);
    if (gp->steel_battalion_in_report.dButtons.Fire)          b |= (1ULL << 1);
    if (gp->steel_battalion_in_report.dButtons.LockOn)        b |= (1ULL << 2);
    if (gp->steel_battalion_in_report.dButtons.Eject)         b |= (1ULL << 3);
    if (gp->steel_battalion_in_report.dButtons.CockpitHatch)  b |= (1ULL << 4);
    if (gp->steel_battalion_in_report.dButtons.Ignition)      b |= (1ULL << 5);
    if (gp->steel_battalion_in_report.dButtons.Start)         b |= (1ULL << 6);
    
    if (gp->steel_battalion_in_report.dButtons.MultiMonitorOpenClose)   b |= (1ULL << 7);
    if (gp->steel_battalion_in_report.dButtons.MultiMonitorMapZoomInOut) b |= (1ULL << 8);
    if (gp->steel_battalion_in_report.dButtons.MultiMonitorModeSelect)  b |= (1ULL << 9);
    if (gp->steel_battalion_in_report.dButtons.MultiMonitorSubMonitor)  b |= (1ULL << 10);
    if (gp->steel_battalion_in_report.dButtons.MainMonitorZoomIn)       b |= (1ULL << 11);
    if (gp->steel_battalion_in_report.dButtons.MainMonitorZoomOut)      b |= (1ULL << 12);
    if (gp->steel_battalion_in_report.dButtons.ForecastShootingSystem)  b |= (1ULL << 13);
    if (gp->steel_battalion_in_report.dButtons.Manipulator)            b |= (1ULL << 14);
    if (gp->steel_battalion_in_report.dButtons.LineColorChange)        b |= (1ULL << 15);
    
    if (gp->steel_battalion_in_report.dButtons.Washing)                b |= (1ULL << 16);
    if (gp->steel_battalion_in_report.dButtons.Extinguisher)           b |= (1ULL << 17);
    if (gp->steel_battalion_in_report.dButtons.Chaff)                  b |= (1ULL << 18);
    if (gp->steel_battalion_in_report.dButtons.TankDetach)             b |= (1ULL << 19);
    if (gp->steel_battalion_in_report.dButtons.Override)               b |= (1ULL << 20);
    if (gp->steel_battalion_in_report.dButtons.NightScope)             b |= (1ULL << 21);
    if (gp->steel_battalion_in_report.dButtons.Function1)              b |= (1ULL << 22);
    if (gp->steel_battalion_in_report.dButtons.Function2)              b |= (1ULL << 23);
    if (gp->steel_battalion_in_report.dButtons.Function3)              b |= (1ULL << 24);
    
    if (gp->steel_battalion_in_report.dButtons.WeaponConMain)          b |= (1ULL << 25);
    if (gp->steel_battalion_in_report.dButtons.WeaponConSub)           b |= (1ULL << 26);
    if (gp->steel_battalion_in_report.dButtons.WeaponConMagazine)      b |= (1ULL << 27);
    if (gp->steel_battalion_in_report.dButtons.Comm1)                  b |= (1ULL << 28);
    if (gp->steel_battalion_in_report.dButtons.Comm2)                  b |= (1ULL << 29);
    if (gp->steel_battalion_in_report.dButtons.Comm3)                  b |= (1ULL << 30);
    if (gp->steel_battalion_in_report.dButtons.Comm4)                  b |= (1ULL << 31);
    if (gp->steel_battalion_in_report.dButtons.Comm5)                  b |= (1ULL << 32);
    
    if (gp->steel_battalion_in_report.dButtons.SightChange)            b |= (1ULL << 33);
    if (gp->steel_battalion_in_report.dButtons.ToggleFiltControl)      b |= (1ULL << 34);
    if (gp->steel_battalion_in_report.dButtons.ToggleOxygenSupply)     b |= (1ULL << 35);
    if (gp->steel_battalion_in_report.dButtons.ToggleFuelFlowRate)     b |= (1ULL << 36);
    if (gp->steel_battalion_in_report.dButtons.ToggleBufferMaterial)   b |= (1ULL << 37);
    if (gp->steel_battalion_in_report.dButtons.ToggleVTLocation)       b |= (1ULL << 38);

out_buf[2] = (uint8_t)(b & 0xFF);
out_buf[3] = (uint8_t)((b >> 8) & 0xFF);
out_buf[4] = (uint8_t)((b >> 16) & 0xFF);
out_buf[5] = (uint8_t)((b >> 24) & 0xFF);
out_buf[6] = (uint8_t)(((b >> 32) & 0x7F) | 0x80);
    out_buf[7] = 0x00;                      // Verification spacing index

    // Map the raw single-byte analog components into their exact offsets
    out_buf[9]  = gp->steel_battalion_in_report.aimingX;
    out_buf[11] = gp->steel_battalion_in_report.aimingY;
    out_buf[13] = gp->steel_battalion_in_report.rotationLever;
    out_buf[15] = gp->steel_battalion_in_report.sightChangeX;
    out_buf[17] = gp->steel_battalion_in_report.sightChangeY;

    // Extract foot pedal analog channels
    out_buf[19] = gp->steel_battalion_in_report.leftPedal;
    out_buf[21] = gp->steel_battalion_in_report.middlePedal;
    out_buf[23] = gp->steel_battalion_in_report.rightPedal;
    
    // Map remaining state metrics 
    out_buf[24] = gp->steel_battalion_in_report.tunerDial & 0x0F;
    out_buf[25] = gp->steel_battalion_in_report.gearLever;
}
