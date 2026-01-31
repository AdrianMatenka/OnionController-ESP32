/**
 * @file onion_ble.h
 * @brief Bluetooth Low Energy (BLE) HID Keyboard module.
 *
 * This module manages the NimBLE stack, HID service registration, 
 * and handles communication with the BLE host (computer/phone).
 */

#ifndef ONION_BLE_H
#define ONION_BLE_H

#include <stdint.h>
#include <stdbool.h>
#include "host/ble_hs.h"
#include "nimble/ble.h"

/** * @brief Reference to the HID Report Map defined in config. 
 */
extern const uint8_t hid_report_map[];

extern uint16_t last_raw_values[16];

/**
 * @brief Starts BLE GAP advertising to make the device discoverable.
 */
void ble_app_advertise(void);

/**
 * @brief Initializes the GATT server and registers HID services.
 * @note Renamed from onion_gatt_svr_init if you followed the name-conflict fix.
 */
void gatt_svr_init(void);
 
/**
 * @brief Constructs and sends a HID keyboard report over BLE.
 * * @param keycode HID scan code to be sent in the report array.
 * @param pressed True for KeyDown event, false for KeyUp event.
 */
void send_key_report(uint8_t keycode, bool pressed);

/**
 * @brief Callback triggered when the BLE host and controller are in sync.
 * Typically used to start advertising.
 */
void ble_app_on_sync(void);

/**
 * @brief Callback triggered when the BLE host is reset.
 * @param reason The code indicating why the reset occurred.
 */
void ble_app_on_reset(int reason);

/**
 * @brief FreeRTOS task that runs the NimBLE host stack.
 * @param param Task parameters (unused).
 */
void ble_host_task(void *param);

/**
 * @brief High-level initialization of the BLE stack and security settings.
 * * Configures device appearance, I/O capabilities, and memory for bonding.
 * @return 0 on success, or error code.
 */
int onion_ble_init(void);

/* Starts a FreeRTOS task that sends raw sensor data over Serial */
void onion_ble_start_telemetry(uint16_t *data_to_watch);

#endif // ONION_BLE_H