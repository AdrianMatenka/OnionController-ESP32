/**
 * @file onion_config.h
 * @brief Global configuration, HID descriptors, and hardware definitions.
 */

#ifndef ONION_CONFIG_H
#define ONION_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/touch_pad.h"

/** @brief Device name advertised over Bluetooth GAP. */
#define DEVICE_NAME "OnionController"

/* --- BLE HID Service UUIDs --- */
#define BLE_SVC_HID_UUID16                          0x1812
#define BLE_SVC_HID_CHR_UUID16_REPORT               0x2a4d
#define BLE_SVC_HID_CHR_UUID16_REPORT_MAP           0x2a4b
#define BLE_SVC_HID_CHR_UUID16_HID_INFO             0x2a4a
#define BLE_SVC_HID_CHR_UUID16_HID_CTRL_POINT        0x2a4c
#define BLE_SVC_HID_CHR_UUID16_PROTOCOL_MODE        0x2a4e

/* --- Hardware Pins & Multiplexer Config --- */
#define MUX_S0 18
#define MUX_S1 19
#define MUX_S2 21
#define MUX_S3 22
#define MUX_INPUT_PIN ADC1_CHANNEL_6
#define MUX_CHANNELS_COUNT 16
#define STATUS_LED_GPIO 2

/** @brief Global default threshold for touch detection. */
#define DEFAULT_THRESHOLD  3900

/**
 * @brief Structure representing a single touch-key mapping.
 */
typedef struct {
    uint8_t  keycode;   /**< HID Keyboard scan code */
    uint16_t threshold; /**< Capacitive touch trigger level */
    bool     is_pressed;/**< Current debounce/state flag */
} onion_key_t;

/**
 * @brief States for the controller power-saving machine.
 */
typedef enum {
    STATE_STANDBY,      /**< Slow polling, low activity */
    STATE_ACTIVE        /**< Fast polling, active user interaction */
} controller_state_t;

#endif // ONION_CONFIG_H