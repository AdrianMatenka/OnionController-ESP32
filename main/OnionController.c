/**
 * @file OnionController.c
 * @brief Main entry point for the Onion Controller HID device.
 * * This file orchestrates the initialization of NVS, Bluetooth (NimBLE),
 * and touch peripherals, and manages the main scanning loop.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "host/util/util.h"
#include "store/config/ble_store_config.h"

// Custom modules headers
#include "onion_config.h"
#include "onion_ble.h"
#include "onion_touch.h"

static const char *TAG = "ONION_MAIN";

/**
 * @brief Application entry point.
 * * Initializes the system components in the required order and runs 
 * the infinite polling loop for touch input and BLE HID reporting.
 */
void app_main(void) {
    /* 1. Initialize system-wide configuration (NVS, storage) */
    onion_config_init();

    /* 2. Initialize Bluetooth HID stack (NimBLE, GATT services) */
    onion_ble_init();

    /* 3. Initialize hardware-specific touch components (MUX and Pads) */
    onion_touch_init();

    ESP_LOGI(TAG, "Controller is ready! Starting main loop.");

    while (1) {
        bool activity_detected = false;

        /* Scan all 16 channels of the multiplexer */
        for (int channel_idx = 0; channel_idx < MUX_CHANNELS_COUNT; channel_idx++) {
            
            /** * @note We check for state changes first to prevent flooding 
             * the BLE stack with redundant HID reports.
             */
            if (onion_touch_has_changed(channel_idx)) {
                bool is_pressed = onion_touch_read(channel_idx);
                
                /* Dispatch the HID report to the connected BLE host */
                send_key_report(onion_lut[channel_idx].keycode, is_pressed);
                
                ESP_LOGD(TAG, "Channel %d: %s", channel_idx, is_pressed ? "Pressed" : "Released");
                activity_detected = true;
            }
        }

        /**
         * Adaptive Task Delay:
         * Provides low-latency response (10ms) during active use,
         * and reduces CPU overhead (50ms) during idle periods.
         */
        vTaskDelay(pdMS_TO_TICKS(activity_detected ? 10 : 50));
    }
}