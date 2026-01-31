/**
 * @file onion_touch.h
 * @brief Touch sensor and Multiplexer management module.
 * * This module handles the hardware abstraction for the capacitive touch 
 * sensors connected via a 16-channel analog multiplexer (MUX).
 */

#ifndef ONION_TOUCH_H
#define ONION_TOUCH_H

#include <stdint.h>
#include <stdbool.h>
#include "onion_config.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

/**
 * @brief Global lookup table containing keycodes and touch thresholds.
 * Defined in onion_touch.c.
 */
extern onion_key_t onion_lut[16];

/**
 * @brief Configures the GPIO address pins for the hardware multiplexer.
 * * @param addr The 4-bit channel address (0-15) to be set on S0-S3 pins.
 */
void set_mux_address(uint8_t addr);

/**
 * @brief Reads the current state of a specific touch channel.
 * * @param channel The MUX channel index to read.
 * @return true if the channel is currently touched (below threshold), false otherwise.
 */
bool onion_touch_read(uint8_t channel);

/**
 * @brief Detects if a touch state change has occurred on a specific channel.
 * * Compares the current raw reading with the previous state stored in onion_lut.
 * * @param channel The MUX channel index to check.
 * @return true if the state (Pressed vs Released) has changed since the last check.
 */
bool onion_touch_has_changed(uint8_t channel);

/**
 * @brief Saves current touch configurations/thresholds to Non-Volatile Storage (NVS).
 * * @return 0 on success, or a non-zero error code.
 */
int onion_config_save(void);

/**
 * @brief Initializes the NVS flash storage and loads saved configurations.
 * * @return 0 on success, or a non-zero error code.
 */
int onion_config_init(void);

/**
 * @brief Performs hardware initialization for the touch pads and MUX GPIOs.
 * * Sets up the touch peripheral clock and configures the MUX selector pins as outputs.
 * * @return 0 on success, or a non-zero error code.
 */
int onion_touch_init(void);

#endif // ONION_TOUCH_H