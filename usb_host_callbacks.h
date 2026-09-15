#ifndef USB_HOST_CALLBACKS_H
#define USB_HOST_CALLBACKS_H

#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"

// Expose the global hardware tracking structures needed by main.c
typedef struct {
    uint8_t dev_addr;
    uint8_t instance;
    bool    needs_activation;
    uint8_t usage_id;  
} ActiveHidDevice_t;

extern volatile ActiveHidDevice_t device_activation_queue[3];

// External queue reference for input packet transfers
typedef struct {
    uint8_t report[64];
    uint16_t len;
    uint8_t usage_id;
    uint8_t dev_addr;
} usb_packet_t;

#define STEEL_BATTALION_REPORT_SIZE 26

// =========================================================================
// SONIK-BR NATIVE STEEL BATTALION STRUCT LAYOUT DEFINITIONS
// =========================================================================
void tuh_sbc_mount_cb(uint8_t dev_addr, uint8_t instance, const sbch_interface_t *sbc_itf);
void tuh_sbc_umount_cb(uint8_t dev_addr, uint8_t instance);
//void tuh_sbc_report_received_cb(uint8_t dev_addr, uint8_t instance, const uint8_t *report, uint16_t len);
void tuh_sbc_report_received_cb(uint8_t dev_addr, uint8_t instance, const uint8_t *report, uint16_t len);
#endif // USB_HOST_CALLBACKS_H
