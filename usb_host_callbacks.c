#include "usb_host_callbacks.h"
#include "neopixel.h"
#include "hid_kbm.h"
#include "pico/util/queue.h"
#include "hardware/gpio.h"
#include "lib/tinyusb/src/tusb.h"
//#include "lib/tinyusb/src/class/sbc/sbc_host.h

#define PICO_LED_PIN 25 

extern queue_t gamepad_packet_queue;
volatile ActiveHidDevice_t device_activation_queue[3] = {0};

// =========================================================================
// SONIK-BR SPECIFIC NATIVE STEEL BATTALION DRIVER CALLBACKS
// =========================================================================


// Triggered automatically when the controller completes its internal "Magic Knock" setup sequence
void tuh_sbc_mount_cb(uint8_t dev_addr, uint8_t instance, const sbch_interface_t *sbc_itf) {
    // Device is fully mounted! Turn NeoPixel to Green or Cyan here to verify.
    //neopixel_set_color(0, 255, 0); 
    neopixel_set_color(255, 69, 0); //ORANGE
    // Request the first input data block immediately
    tuh_sbc_receive_report(dev_addr, instance);
}

// Triggered automatically when the controller disconnects
void tuh_sbc_umount_cb(uint8_t dev_addr, uint8_t instance) {
    neopixel_set_color(255, 0, 0); // Turn Red when removed
}

/* // Triggered automatically when raw report data arrives over the USB pipe
void tuh_sbc_report_received_cb(uint8_t dev_addr, uint8_t instance, const uint8_t *report, uint16_t len) {
    // Cast the raw input report safely using the library's pre-defined layout struct
    sbch_interface_t *xid_itf = (sbch_interface_t *)report;
    sbc_gamepad_t *pad = &xid_itf->pad;

    if (xid_itf->connected && xid_itf->new_pad_data) {
        // Your logic to process buttons and map axis dials goes here!
        // Example: if (pad->wButtons & SBC_BUTTON_FIRE) { ... }
    }

    // Keep the polling loop active by preparing the endpoint for the next packet
    tuh_sbc_receive_report(dev_addr, instance);
} */



// Triggered automatically when raw report data arrives over the USB pipe
void tuh_sbc_report_received_cb(uint8_t dev_addr, uint8_t instance, const uint8_t *report, uint16_t len) {
    // Cast the raw input report safely using the library's pre-defined layout struct
    sbch_interface_t *xid_itf = (sbch_interface_t *)report;

    if (xid_itf->connected && xid_itf->new_pad_data) {
        // Construct a tracking packet to push across to Core 0
        usb_packet_t sbc_pkt;
        sbc_pkt.len = (len > 64) ? 64 : len;
        sbc_pkt.usage_id = 0x80; // Special tag identifying physical SBC hardware
        sbc_pkt.dev_addr = dev_addr;
        memcpy(sbc_pkt.report, report, sbc_pkt.len);

        // Safely push the frame over to Core 0's processing queue
        queue_try_add(&gamepad_packet_queue, &sbc_pkt);
    }

    // Keep the polling loop active by preparing the endpoint for the next packet
    tuh_sbc_receive_report(dev_addr, instance);
}



// =========================================================================
// STANDARD CLASS HID DRIVER CALLBACKS (For Keyboards, Mice, and Gamepads)
// =========================================================================
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    uint8_t usage_id = 0;
    uint16_t vid = 0, pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    if (vid == 0x054c && (pid == 0x09cc || pid == 0x05c4)) {
        usage_id = 0x99; // PS4 Gamepad identifier tag
    } 
    else if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        usage_id = HID_ITF_PROTOCOL_KEYBOARD;
    }
    else if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        usage_id = HID_ITF_PROTOCOL_MOUSE;
    }
    else if (itf_protocol == HID_ITF_PROTOCOL_NONE && desc_report != NULL && desc_len > 0) {
        tuh_hid_report_info_t report_info[3]; 
        uint8_t report_count = tuh_hid_parse_report_descriptor(report_info, 3, desc_report, desc_len);
        for (uint8_t i = 0; i < report_count; i++) {
            if (report_info[i].usage_page == 0x01) { 
                if (report_info[i].usage == 0x02) { usage_id = HID_ITF_PROTOCOL_MOUSE; break; }
                else if (report_info[i].usage == 0x04 || report_info[i].usage == 0x05) { usage_id = report_info[i].usage; break; }
            }
        }
    } else {
        usage_id = itf_protocol;
    }

    if (usage_id == 0) return;
																						//Orange if Steel Battalion
/*     if (usage_id == HID_ITF_PROTOCOL_KEYBOARD)      neopixel_set_color(255, 0, 0);     // Red if Keyboard
    else if (usage_id == HID_ITF_PROTOCOL_MOUSE)    neopixel_set_color(0, 0, 255);    // Blue if Mouse
    else if (usage_id == 0x99)                      neopixel_set_color(0, 255, 0);    // Green if Gamepad
    else if (usage_id == 0x05)                      neopixel_set_color(255, 200, 0);  // Yellow 
    else                                            neopixel_set_color(255, 255, 255); // White */
    if (usage_id == HID_ITF_PROTOCOL_KEYBOARD)      neopixel_set_color(255, 0, 0);     // Red if Keyboard
    else if (usage_id == HID_ITF_PROTOCOL_MOUSE)    neopixel_set_color(0, 0, 255);    // Blue if Mouse
    else if (usage_id == 0x99)                      neopixel_set_color(0, 255, 0);    // Green if Gamepad (PS4)
    else if (usage_id == 0x05 || usage_id == 0x04)  neopixel_set_color(128, 0, 128);  // Purple if Generic Joystick/Gamepad
    else if (usage_id == 0x01)                      neopixel_set_color(255, 200, 0);  // Yellow 
    else                                            neopixel_set_color(255, 255, 255); // White




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
            memset((void*)&device_activation_queue[i], 0, sizeof(ActiveHidDevice_t));
            break;
        }
    }
    neopixel_set_color(20, 20, 20); 
}

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
    } 
    else if (usage == HID_ITF_PROTOCOL_MOUSE) {
        process_hid_mouse(report, len);
    }
    // --- ADD THIS BLOCK TO PROCESS GENERIC JOYSTICKS/GAMEPADS ---
    else if (usage == 0x04 || usage == 0x05 || usage == 0x99) {
        // Construct the tracking packet matching your main.c structural layout
        usb_packet_t joystick_pkt;
        joystick_pkt.len = (len > 64) ? 64 : len;
        joystick_pkt.usage_id = usage;
        joystick_pkt.dev_addr = dev_addr;
        memcpy(joystick_pkt.report, report, joystick_pkt.len);

        // Safely push the frame over to Core 0's processing queue
        queue_try_add(&gamepad_packet_queue, &joystick_pkt);
    }

    tuh_hid_receive_report(dev_addr, instance);
}

