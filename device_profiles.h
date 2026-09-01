#ifndef DEVICE_PROFILES_H
#define DEVICE_PROFILES_H

#include <stdint.h>
#include <stdbool.h>
#include "hid_gamepad.h"

// Unified profile identification tags
typedef enum {
    PROFILE_GENERIC_HID = 0,
    PROFILE_SONY_DS4,
    PROFILE_LOGITECH_DUAL_ACTION,
    PROFILE_STEEL_BATTALION,      // Native direct-pass profile for VT simulation setups
    PROFILE_DEBUG_RAW_DUMP        // Fallback for simulation gear to capture data fields
} gamepad_profile_id_t;

// Structural registry entry
typedef struct {
    uint16_t vid;
    uint16_t pid;
    gamepad_profile_id_t profile_id;
    const char* category;
    const char* name;
} gamepad_device_id_t;

// Central Look-Up Registry for Hardware Identification Tracking
static const gamepad_device_id_t GAMEPAD_REGISTRY[] = {
    // === NATIVE EMULATED SIMULATION GEAR ===
    { 0x0A7B, 0xD000, PROFILE_STEEL_BATTALION,     "Simulation",   "Emulated Steel Battalion Controller" },

    // === SONY CONSOLES ===
    { 0x054C, 0x05C4, PROFILE_SONY_DS4,            "Gamepad",      "Sony DualShock 4 V1" },
    { 0x054C, 0x09CC, PROFILE_SONY_DS4,            "Gamepad",      "Sony DualShock 4 V2" },
    { 0x054C, 0x0CE6, PROFILE_DEBUG_RAW_DUMP,       "Gamepad",      "Sony DualSense PS5" },
    { 0x054C, 0x0268, PROFILE_DEBUG_RAW_DUMP,       "Gamepad",      "Sony DualShock 3 (Sixaxis)" },

    // === MICROSOFT / XBOX CONSOLES ===
    { 0x045E, 0x028E, PROFILE_DEBUG_RAW_DUMP,       "Gamepad",      "Xbox 360 Wired Controller" },
    { 0x045E, 0x02D1, PROFILE_DEBUG_RAW_DUMP,       "Gamepad",      "Xbox One Controller (Early)" },
    { 0x045E, 0x02EA, PROFILE_DEBUG_RAW_DUMP,       "Gamepad",      "Xbox One S Controller (USB)" },
    { 0x045E, 0x0B12, PROFILE_DEBUG_RAW_DUMP,       "Gamepad",      "Xbox Series X/S Controller" },

    // === NINTENDO SYSTEM PERIPHERALS ===
    { 0x057E, 0x2009, PROFILE_DEBUG_RAW_DUMP,       "Gamepad",      "Nintendo Switch Pro Controller" },
    { 0x057E, 0x0337, PROFILE_DEBUG_RAW_DUMP,       "Arcade/Retro", "Wii U / Switch GameCube Adapter" },

    // === LOGITECH ECOSYSTEM ===
    { 0x046D, 0xC216, PROFILE_LOGITECH_DUAL_ACTION, "Gamepad",      "Logitech Dual Action" },
    { 0x046D, 0xC21F, PROFILE_DEBUG_RAW_DUMP,       "Gamepad",      "Logitech F710 Wireless Gamepad" },
    { 0x046D, 0xC21D, PROFILE_DEBUG_RAW_DUMP,       "Gamepad",      "Logitech F310 Wired Gamepad" },
    { 0x046D, 0xC215, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "Logitech Extreme 3D Pro" },
    { 0x046D, 0xC29B, PROFILE_DEBUG_RAW_DUMP,       "Racing Wheel", "Logitech G27 Racing Wheel" },
    { 0x046D, 0xC24F, PROFILE_DEBUG_RAW_DUMP,       "Racing Wheel", "Logitech G29 Driving Force" },
    { 0x046D, 0xC262, PROFILE_DEBUG_RAW_DUMP,       "Racing Wheel", "Logitech G923 Wheel (PlayStation)" },

    // === THRUSTMASTER FLIGHT & RACING SIMULATION ===
    { 0x044F, 0xB10A, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "Thrustmaster Warthog Joystick" },
    { 0x044F, 0xB108, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "Thrustmaster Warthog Throttle" },
    { 0x044F, 0xB679, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "Thrustmaster T.16000M FCS" },
    { 0x044F, 0xB687, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "Thrustmaster TWCS Throttle" },
    { 0x044F, 0xB66D, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "Thrustmaster T-Flight Hotas X" },
    { 0x044F, 0xB692, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "Thrustmaster TCA Sidestick Airbus" },
    { 0x044F, 0xB65E, PROFILE_DEBUG_RAW_DUMP,       "Racing Wheel", "Thrustmaster T300RS Wheel" },

    // === SAITEK / CH PRODUCTS / MAD CATZ ===
    { 0x06A3, 0x075C, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "Saitek X52 Flight Control System" },
    { 0x06A3, 0x0BAC, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "Saitek X56 Rhino HOTAS" },
    { 0x068E, 0x00FF, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "CH Products Combatstick" },
    { 0x068E, 0x00F1, PROFILE_DEBUG_RAW_DUMP,       "Flight Stick", "CH Products Pro Throttle" },

    // === ADVANCED RACING HARDWARE (FANATEC / SIMUCUBE) ===
    { 0x0EB7, 0x0001, PROFILE_DEBUG_RAW_DUMP,       "Racing Wheel", "Fanatec ClubSport Wheel Base" },
    { 0x16D0, 0x0D5A, PROFILE_DEBUG_RAW_DUMP,       "Racing Wheel", "Simucube 2 Sport/Pro Base" },

    // === RETRO / ARCADE / POPULAR THIRD-PARTY ACCESORIES ===
    { 0x2DC8, 0x6101, PROFILE_DEBUG_RAW_DUMP,       "Gamepad",      "8BitDo SN30 Pro USB" },
    { 0x2DC8, 0x1003, PROFILE_DEBUG_RAW_DUMP,       "Arcade/Retro", "8BitDo Arcade Stick" },
    { 0x0F0D, 0x00aa, PROFILE_DEBUG_RAW_DUMP,       "Arcade/Retro", "Hori Fighting Commander" },
    { 0x1209, 0x2328, PROFILE_DEBUG_RAW_DUMP,       "Arcade/Retro", "Generic Brook Fighting Board" }
};

#define GAMEPAD_REGISTRY_COUNT (sizeof(GAMEPAD_REGISTRY) / sizeof(GAMEPAD_REGISTRY))

// Execution and Routing Pipeline Signatures
bool route_and_parse_gamepad(uint8_t const* report, uint16_t len, uint8_t dev_addr, generic_gamepad_data_t* out_data);
bool parse_logitech_dual_action(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);
bool parse_steel_battalion_native(uint8_t const* report, uint16_t len, generic_gamepad_data_t* out_data);

#endif // DEVICE_PROFILES_H
