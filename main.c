#include "tusb_gamepad.h"  // <--- ADD THIS LINE HERE TO DEFINE THE GAMEPAD TYPE
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

/**
 * Calculates a smooth relative delta value for a trackpad axis.
 * Protects memory across empty queue frames.
 */
int16_t calculate_relative_axis(uint16_t current_pos, uint16_t* last_pos, bool finger_active, bool packet_fresh) {
    static bool was_touching = false;
    int32_t output_delta = 0;
    
    // Sensitivity Scaling Multiplier (Adjust this to fine-tune camera tracking speed)
    const int32_t sensitivity = 450; 

    // We ONLY update calculations when a fresh USB packet actually arrives
    if (packet_fresh) {
        if (finger_active) {
            if (was_touching) {
                // Compute the signed physical distance traveled since the last packet
                int16_t raw_delta = (int16_t)current_pos - (int16_t)(*last_pos);

                // Defensive Jitter Deadzone: Discard minor electrical tracking noise (1-2 pixels)
                if (abs(raw_delta) >= 2) {
                    output_delta = raw_delta * sensitivity;
                }
            }
            // Lock in this coordinate as the historical baseline anchor for the next packet
            *last_pos = current_pos;
            was_touching = true;
        } else {
            was_touching = false;
        }
    }

    // Clamp parameters cleanly into the Steel Battalion signed 16-bit range (-32767 to 32767)
    if (output_delta > 32767)  output_delta = 32767;
    if (output_delta < -32767) output_delta = -32767;

    return (int16_t)output_delta;
}



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

//bool persistent_touch_active = false;

    while (1) {
        // Sync local tracking states from shared global values safely
        local_modifiers = global_modifiers;
        for (int i = 0; i < 6; i++) local_keycodes[i] = global_keycodes[i];
        local_mouse_buttons = global_mouse_buttons;
        local_mouse_x       = global_mouse_x;
        local_mouse_y       = global_mouse_y;
        local_mouse_wheel   = global_mouse_wheel;


        // 2. Clear global frames and activities
        global_mouse_x = 0; global_mouse_y = 0; global_mouse_wheel = 0;
        gamepad_activity = false;

        // 3. FORCE ZERO local_joy_data here so it can never hold stale touch data or ignore inputs
        memset(&local_joy_data, 0, sizeof(generic_gamepad_data_t));
        local_joy_data.hat = 8; // Default hat to idle release



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
				//persistent_touch_active = local_joy_data.finger_active;
                // CORRECTION: Check if D-pad is active (anything except 8) OR if any digital buttons are clicked
                bool button_or_hat_active = (local_joy_data.buttons != 0) || (local_joy_data.hat != 8);
				bool touch_active = (local_joy_data.finger_active);
                // CORRECTION: Apply defensive deadzones to your newly mapped LX and LY variables
                bool analog_sticks_active = (abs(local_joy_data.lx) > 15) || (abs(local_joy_data.ly) > 15) ||
                                            (abs(local_joy_data.rx) > 15) || (abs(local_joy_data.ry) > 15);
				bool z_axis_active = (abs(local_joy_data.z_trigger != 0));
				//(abs(local_joy_data.finger_x>8) || abs(local_joy_data.finger_x>4));
				//bool touch_active = (abs(local_joy_data.touch0.active != 1);
				
                gamepad_activity = button_or_hat_active || analog_sticks_active || z_axis_active || touch_active;
            }
        }


        // --- CONSOLIDATED STEEL BATTALION MAPPING MACHINE ---
        apply_inputs_to_steel_battalion(gp, &local_joy_data, gamepad_activity);

        // Core 0 updates activity signaling LED status
		//LED ACTIVE
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

/*     if (pad_data->finger_active) {
        // 1. Normalize down to an accurate percentage ratio (0.0 to 1.0) using correct max bounds
        float ratio_x = (float)pad_data->finger_x / 1919.0f;
        float ratio_y = (float)pad_data->finger_y / 941.0f; // Fixed vertical height boundary
        
        // 2. Scale cleanly into the full unsigned 16-bit range (0 to 65535) expected by the emulator
        uint16_t scaled_sight_x = (uint16_t)(ratio_x * 65535.0f);
        uint16_t scaled_sight_y = (uint16_t)(ratio_y * 65535.0f);
        
        // 3. Assign the integer outputs safely
        gp->steel_battalion_in_report.sightChangeX = scaled_sight_x;
        gp->steel_battalion_in_report.sightChangeY = scaled_sight_y;
    } */
	
	
	if (pad_data->hat == 0)   gp->steel_battalion_in_report.sightChangeY = -8000;
    if (pad_data->hat == 4)   gp->steel_battalion_in_report.sightChangeY = 8000;
	if (pad_data->hat == 2)	  gp->steel_battalion_in_report.sightChangeX = 8000;
    if (pad_data->hat == 6)	  gp->steel_battalion_in_report.sightChangeY = -8000;
	
	
/* 	    // 2. Memory anchors that live safely across loop passes (protected from your memset zone)
    static uint16_t track_history_x = 0;
    static uint16_t track_history_y = 0;

    // 3. Compute relative movement smoothly via our decoupled helper function
    gp->steel_battalion_in_report.sightChangeX = calculate_relative_axis(
        pad_data->finger_x, 
        &track_history_x, 
        pad_data->finger_active, 
        dynamic_pad_active
    );

    gp->steel_battalion_in_report.sightChangeY = calculate_relative_axis(
        pad_data->finger_y, 
        &track_history_y, 
        pad_data->finger_active, 
        dynamic_pad_active
    ); */
	
	
	
	
    // 2. Keyboard Rotation Lever Mapping
    if (is_key_pressed(KEY_A)) gp->steel_battalion_in_report.rotationLever = -32768; 
    else if (is_key_pressed(KEY_D)) gp->steel_battalion_in_report.rotationLever = 32767;  
    else gp->steel_battalion_in_report.rotationLever = 0;      

    // 3. Mouse Integration Processing Calculations
    accumulated_aim_x += (local_mouse_x * MOUSE_SENSITIVITY);
    accumulated_aim_y += (local_mouse_y * MOUSE_SENSITIVITY);

    // 4. Overlap Analog Stick Data if a Hardware Gamepad is Actively Pushed
    // 4. Overlap Analog Stick Data if a Hardware Gamepad is Actively Pushed
    if (dynamic_pad_active) {
        // Defensive filtering checks using Left Stick coordinates
        int8_t fx = (abs(pad_data->lx) > 15) ? pad_data->lx : 0;
        int8_t fy = (abs(pad_data->ly) > 15) ? pad_data->ly : 0;
        
        gp->steel_battalion_in_report.aimingX = (uint16_t)((fx + 128) << 8);
        gp->steel_battalion_in_report.aimingY = (uint16_t)((fy + 128) << 8);
        
        // Gamepad direct button overrides mapped onto structural masks
        if (pad_data->buttons & (1 << 0)) gp->steel_battalion_in_report.dButtons.MainWeapon = true; // Cross
        if (pad_data->buttons & (1 << 1)) gp->steel_battalion_in_report.dButtons.Fire       = true; // Circle
	    if (pad_data->buttons & (1 << 3)) gp->steel_battalion_in_report.dButtons.LockOn       = true; // Triangle
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

    // 6. Increment Time Vector Tracking Metric
    t++;
}

// Keep core1_usb_host_entry(), tuh_hid_mount_cb() hub logic below...
// =========================================================================
// CORE 1: ASYNCHRONOUS USB HOST PACKET HARVESTER
// =========================================================================

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
                tuh_hid_receive_report(device_activation_queue[i].dev_addr, device_activation_queue[i].instance);
                device_activation_queue[i].needs_activation = false;
            }
        }
    }
}

// Helper routine to look up Sony DualShock 4 signatures
static inline bool check_sony_device(uint8_t dev_addr) {
    uint16_t vid, pid; 
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    return (vid == 0x054c && (pid == 0x09cc || pid == 0x05c4));
}

// TinyUSB callback: Triggered when an HID device is attached
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    uint8_t usage_id = 0;

    if (check_sony_device(dev_addr)) {
        usage_id = 0x99; // Assign our custom identification tag for PS4/DS4
    } else if (itf_protocol == HID_ITF_PROTOCOL_NONE) {
        tuh_hid_report_info_t report_info[3]; 
        uint8_t report_count = tuh_hid_parse_report_descriptor(report_info, 3, desc_report, desc_len);
        for (uint8_t i = 0; i < report_count; i++) {
            if (report_info[i].usage_page == 0x01 && (report_info[i].usage == 0x04 || report_info[i].usage == 0x05)) {
                usage_id = report_info[i].usage; 
                break;
            }
        }
    } else {
        usage_id = itf_protocol; // Standard Keyboard (1) or Mouse (2)
    }

    if (usage_id == 0) return;

    // Place the detected device into an open hardware slot
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

// TinyUSB callback: Triggered when an HID device is pulled out
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    for (int i = 0; i < 3; i++) {
        if (device_activation_queue[i].dev_addr == dev_addr && device_activation_queue[i].instance == instance) {
            memset((void*)&device_activation_queue[i], 0, sizeof(ActiveHidDevice_t));
            break;
        }
    }
}

// TinyUSB callback: Triggered when an HID interrupt report packet arrives
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    uint8_t usage = 0;
    for (int i = 0; i < 3; i++) {
        if (device_activation_queue[i].dev_addr == dev_addr && device_activation_queue[i].instance == instance) {
            usage = device_activation_queue[i].usage_id; 
            break;
        }
    }

    if (usage == HID_ITF_PROTOCOL_KEYBOARD) {
        process_hid_keyboard(report, len);
    } else if (usage == HID_ITF_PROTOCOL_MOUSE) {
        process_hid_mouse(report, len);
    } else if (usage == 0x04 || usage == 0x05 || usage == 0x99) {
        usb_packet_t packet;
        packet.len = (len > 64) ? 64 : len;
        packet.usage_id = usage;
        packet.dev_addr = dev_addr;
        memcpy(packet.report, report, packet.len);
        queue_try_add(&gamepad_packet_queue, &packet);
    }
    tuh_hid_receive_report(dev_addr, instance);
}
