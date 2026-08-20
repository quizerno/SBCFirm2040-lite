#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

#ifndef CFG_TUSB_MCU
  #error CFG_TUSB_MCU must be defined
#endif

// --- NATIVE XBOX DEVICE PORT ---
#define BOARD_DEVICE_RHPORT_NUM     0
#define BOARD_DEVICE_RHPORT_SPEED   OPT_MODE_FULL_SPEED
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE | BOARD_DEVICE_RHPORT_SPEED)

// --- PIO KEYBOARD HOST PORT ---
#define BOARD_HOST_RHPORT_NUM       1
#define BOARD_HOST_RHPORT_SPEED     OPT_MODE_LOW_SPEED 
#define CFG_TUSB_RHPORT1_MODE       (OPT_MODE_HOST | BOARD_HOST_RHPORT_SPEED)

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS                 OPT_OS_NONE
#endif

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION (Xbox / Steel Battalion output)
//--------------------------------------------------------------------
#define CFG_TUD_ENABLED           1  
#define CFG_TUD_HID               1
#define CFG_TUH_RPI_PIO_USB		  1

//Hub related
#define CFG_TUH_HUB 			  1
#define CFG_TUH_DEVICE_MAX        4
//(4 * CFG_TUH_HUB + 1)

// Enforced dependencies for the gamepad framework
#define CFG_TUD_CDC               1  
#define CFG_TUD_CDC_TX_BUFSIZE    0  
#define CFG_TUD_CDC_RX_BUFSIZE    0  

#define CFG_TUD_MSC               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0

#ifdef CFG_TUD_HID_EP_BUFSIZE
#undef CFG_TUD_HID_EP_BUFSIZE
#endif
#define CFG_TUD_HID_EP_BUFSIZE    64 

//--------------------------------------------------------------------
// HOST CONFIGURATION (Keyboard Reader Input)
//--------------------------------------------------------------------
#define CFG_TUH_ENABLED           1  
#define CFG_TUH_HID               4  
#define CFG_TUH_HID_EP_BUFSIZE    64
#define CFG_TUH_ENDPOINT_TO_PROCESS 2  


//HOST STACK
// Increase the buffer pool size allocated for capturing incoming device descriptors
#ifndef CFG_TUH_ENUMERATION_BUFSIZE
#define CFG_TUH_ENUMERATION_BUFSIZE    512  // Increase from default 256 to 512
#endif

// Ensure TinyUSB handles complex multiple-axis interfaces concurrently
#ifndef CFG_TUD_HID_BUFSIZE
#define CFG_TUD_HID_BUFSIZE            64   // Set to 64 bytes
#endif


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
#define MAX_GAMEPADS 1
