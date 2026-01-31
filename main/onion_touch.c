/**
 * @file onion_touch.c
 * @brief Implementation of touch sensing, multiplexer control, and NVS configuration.
 */

#include "onion_touch.h"
#include "onion_config.h"
#include "onion_ble.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_rom_sys.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"

/* Logging and NVS Storage constants */
static const char *TAG = "ONION_CONFIG";
static const char *NVS_NAMESPACE = "onion_storage";
static const char *NVS_KEY_LUT = "onion_lut";

/** * @brief Internal state tracker to identify changes between polling cycles.
 */
static bool last_states[16] = {0};

/**
 * @brief Default Lookup Table (LUT) containing HID keycodes and touch thresholds.
 * @note This table is overwritten if valid data is found in NVS.
 */
onion_key_t onion_lut[16] = {
    {0x1A, DEFAULT_THRESHOLD, false}, {0x16, DEFAULT_THRESHOLD, false}, {0x04, DEFAULT_THRESHOLD, false}, {0x07, DEFAULT_THRESHOLD, false},
    {0x2C, DEFAULT_THRESHOLD, false}, {0x08, DEFAULT_THRESHOLD, false}, {0x0B, DEFAULT_THRESHOLD, false}, {0x0A, DEFAULT_THRESHOLD, false},
    {0x14, DEFAULT_THRESHOLD, false}, {0x2B, DEFAULT_THRESHOLD, false}, {0x4F, DEFAULT_THRESHOLD, false}, {0x50, DEFAULT_THRESHOLD, false},
    {0x52, DEFAULT_THRESHOLD, false}, {0x51, DEFAULT_THRESHOLD, false}, {0x1F, DEFAULT_THRESHOLD, false}, {0x29, DEFAULT_THRESHOLD, false}
};

/**
 * @brief Switches the 16-channel multiplexer to the requested channel.
 * @param addr Target channel address (0-15).
 */
void set_mux_address(uint8_t addr) {
    gpio_set_level(MUX_S0, (addr >> 0) & 0x01);
    gpio_set_level(MUX_S1, (addr >> 1) & 0x01);
    gpio_set_level(MUX_S2, (addr >> 2) & 0x01);
    gpio_set_level(MUX_S3, (addr >> 3) & 0x01);

    esp_rom_delay_us(100);
}

/**
 * @brief Performs a raw touch read on a specific MUX channel.
 * @param channel The MUX channel to evaluate.
 * @return true if touched (raw value < threshold), false otherwise.
 */
bool onion_touch_read(uint8_t channel) {
    set_mux_address(channel);
    
    esp_rom_delay_us(20); 

    uint32_t raw_sum = 0;
    for(int i = 0; i < 4; i++) {
        raw_sum += adc1_get_raw(MUX_INPUT_PIN);
    }
    uint16_t adc_raw = (uint16_t)(raw_sum >> 2);
    
    last_raw_values[channel] = adc_raw;
    
    return (adc_raw < onion_lut[channel].threshold);
}

/**
 * @brief State-machine helper to detect press/release transitions.
 * @param channel The MUX channel to check.
 * @return true if the state changed since the last call.
 */
bool onion_touch_has_changed(uint8_t channel) {
    bool current_state = onion_touch_read(channel);
    if (current_state != last_states[channel]) {
        last_states[channel] = current_state;
        return true;
    }
    return false;
}

/**
 * @brief Opens NVS and loads the onion_lut blob.
 * @return ESP_OK on success, or appropriate error code.
 */
int onion_config_init(void){
    nvs_handle_t handle;
    esp_err_t err;

    /* Initialize NVS partition if not already done by app_main */
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS Open failed (%s). Fallback to hardcoded defaults.", esp_err_to_name(err));
        return err;
    }

    size_t required_size = sizeof(onion_lut);
    err = nvs_get_blob(handle, NVS_KEY_LUT, onion_lut, &required_size);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Factory reset: No stored config. Provisioning NVS...");
        onion_config_save();
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS Data Corruption (%s)", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Configuration synced from NVS storage.");
    }

    nvs_close(handle);
    return ESP_OK;
}

/**
 * @brief Commits the current onion_lut state to persistent storage.
 * @return ESP_OK on success.
 */
int onion_config_save(void) {
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(handle, NVS_KEY_LUT, onion_lut, sizeof(onion_lut));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        ESP_LOGI(TAG, "Flash update successful.");
    }

    nvs_close(handle);
    return err;
}

/**
 * @brief Configures GPIOs and the touch peripheral hardware.
 * @return 0 on success.
 */
int onion_touch_init(void) {
    /* Set MUX address lines as digital outputs */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << MUX_S0) | (1ULL << MUX_S1) | 
                        (1ULL << MUX_S2) | (1ULL << MUX_S3)
    };
    gpio_config(&io_conf);

    adc1_config_width(ADC_WIDTH_BIT_12); // Rozdzielczość 0-4095
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); // Zakres do ok. 3.3V

    ESP_LOGI(TAG, "ADC i MUX logic initialized.");

    gpio_reset_pin(STATUS_LED_GPIO);
    gpio_set_direction(STATUS_LED_GPIO, GPIO_MODE_OUTPUT);

    return 0;
}