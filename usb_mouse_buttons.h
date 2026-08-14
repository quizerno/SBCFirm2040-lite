#ifndef USB_MOUSE_BUTTONS_H
#define USB_MOUSE_BUTTONS_H

// --- STANDARD HID MOUSE BUTTON BITMASKS ---
#define MOUSE_BUTTON_NONE   0x00
#define MOUSE_BUTTON_LEFT   0x01  // Standard Left Click
#define MOUSE_BUTTON_RIGHT  0x02  // Standard Right Click
#define MOUSE_BUTTON_MIDDLE 0x04  // Scroll Wheel Click
#define MOUSE_BUTTON_SIDE1  0x08  // Backward Navigation Side Button (Button 4)
#define MOUSE_BUTTON_SIDE2  0x10  // Forward Navigation Side Button (Button 5)

// --- VIRTUAL SIGNATURE MARKERS FOR SCROLL DIRECTIONS ---
// These use upper bits outside standard 5-button layouts so they 
// can be passed cleanly into your macro matching evaluation loops.
#define MOUSE_SCROLL_UP     0x20
#define MOUSE_SCROLL_DOWN   0x40

#endif // USB_MOUSE_BUTTONS_H