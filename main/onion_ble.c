/**
 * @file onion_ble.c
 * @brief Implementation of BLE HID services and NimBLE stack management.
 */

#include "onion_ble.h"
#include "onion_config.h"
#include "onion_touch.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "store/config/ble_store_config.h"
#include "driver/gpio.h"
#include "string.h"

/* Forward declaration for private storage initialization */
extern void ble_store_config_init(void);

/**
 * @brief HID Report Descriptor for a standard Keyboard profile.
 * Defines the structure of data sent to the host (modifiers, reserved, 6 keycodes).
 */
const uint8_t hid_report_map[] = {
    0x05, 0x01, /* Usage Page (Generic Desktop) */
    0x09, 0x06, /* Usage (Keyboard) */
    0xa1, 0x01, /* Collection (Application) */
    0x85, 0x01, /* Report ID (1) */
    0x05, 0x07, /* Usage Page (Key Codes) */
    0x19, 0xe0, /* Usage Minimum (224 - Left Control) */
    0x29, 0xe7, /* Usage Maximum (231 - Right GUI) */
    0x15, 0x00, /* Logical Minimum (0) */
    0x25, 0x01, /* Logical Maximum (1) */
    0x75, 0x01, /* Report Size (1) */
    0x95, 0x08, /* Report Count (8) */
    0x81, 0x02, /* Input (Data, Variable, Absolute) - Modifier byte */
    0x95, 0x01, /* Report Count (1) */
    0x75, 0x08, /* Report Size (8) */
    0x81, 0x01, /* Input (Constant) - Reserved byte */
    0x95, 0x06, /* Report Count (6) */
    0x75, 0x08, /* Report Size (8) */
    0x15, 0x00, /* Logical Minimum (0) */
    0x25, 0x65, /* Logical Maximum (101) */
    0x05, 0x07, /* Usage Page (Key Codes) */
    0x19, 0x00, /* Usage Minimum (0) */
    0x29, 0x65, /* Usage Maximum (101) */
    0x81, 0x00, /* Input (Data, Array) - Key array */
    0xc0        /* End Collection */
};

static const char *TAG = "ONION_BLE";

/** @brief Local storage for BLE address type and connection handles */
uint8_t addr_type;
uint16_t conn_handle = 0xFFFF;
uint16_t report_handle;
static uint16_t *telemetry_source = NULL;
static onion_key_t *local_lut_ptr = NULL;
static bool is_app_connected = false;


uint16_t last_raw_values[16] = {0};

/* Private function prototypes */
static int gatt_svr_chr_access_hid(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ble_gap_event(struct ble_gap_event *event, void *arg);

/**
 * @brief GATT Service Definitions
 * Includes Device Information Service (DIS) and Human Interface Device (HID).
 */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Device Information ***/
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180a),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid = BLE_UUID16_DECLARE(0x2a29),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0,
        } },
    },
    {
        /*** Service: Human Interface Device (Keyboard) ***/
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_HID_UUID16),
        .characteristics = (struct ble_gatt_chr_def[]) { 
        {
            /* 1. Report Map */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HID_CHR_UUID16_REPORT_MAP),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /* 2. Keyboard Input Report */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HID_CHR_UUID16_REPORT),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &report_handle,
            .descriptors = (struct ble_gatt_dsc_def[]) { {
                .uuid = BLE_UUID16_DECLARE(0x2908), /* Report Reference Descriptor */
                .access_cb = gatt_svr_chr_access_hid,
                .att_flags = BLE_ATT_F_READ,
            }, {
                0,
            } },
        }, {
            /* 3. HID Information */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HID_CHR_UUID16_HID_INFO),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ,
        },
        {
            /* 4. HID Control Point */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HID_CHR_UUID16_HID_CTRL_POINT),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
        }, {
            /* 5. Protocol Mode */
            .uuid = BLE_UUID16_DECLARE(BLE_SVC_HID_CHR_UUID16_PROTOCOL_MODE),
            .access_cb = gatt_svr_chr_access_hid,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP,
        }, {
            0,
        } },
    },
    {
        0,
    },
};

/**
 * @brief Sends a 8-byte HID keyboard report.
 * Format: [modifiers, reserved, key1, key2, key3, key4, key5, key6]
 */
void send_key_report(uint8_t keycode, bool pressed) {
    if (conn_handle == 0xFFFF) return;
    uint8_t report[8] = {0};
    if (pressed) report[2] = keycode; /* Map keycode to the first key slot */
    struct os_mbuf *om = ble_hs_mbuf_from_flat(report, sizeof(report));
    ble_gatts_notify_custom(conn_handle, report_handle, om);
}

/**
 * @brief Configures and starts the BLE advertising process.
 */
void ble_app_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;
    fields.appearance = 0x03C1;
    fields.appearance_is_present = 1;
    fields.uuids16 = (ble_uuid16_t[]){ BLE_UUID16_INIT(BLE_SVC_HID_UUID16) };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    ble_gap_adv_set_fields(&fields);
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 0;
    adv_params.itvl_max = 0;
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

/**
 * @brief Handles GAP events like connection, disconnection, and security.
 */
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "Connection established. Handle: %d", conn_handle);
                ble_gap_security_initiate(conn_handle);
                gpio_set_level(STATUS_LED_GPIO, 1);
            } else {
                ble_app_advertise();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            conn_handle = 0xFFFF;
            ESP_LOGI(TAG, "Device disconnected. Restarting advertising.");
            gpio_set_level(STATUS_LED_GPIO, 0);
            ble_app_advertise();
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            ESP_LOGI(TAG, "Encryption status changed: %d", event->enc_change.status);
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated to: %d", event->mtu.value);
            break;
        case BLE_GAP_EVENT_PASSKEY_ACTION: {
            struct ble_sm_io pk;
            ESP_LOGI(TAG, "Passkey Action; type=%d", event->passkey.params.action);
            if (event->passkey.params.action == BLE_SM_IO_CAP_NO_IO) {
                pk.action = event->passkey.params.action;
                ble_sm_inject_io(event->passkey.conn_handle, &pk);
            }
            break;
        }
    }
    return 0;
}

/**
 * @brief Access callback for GATT characteristics (Reads/Writes).
 */
static int gatt_svr_chr_access_hid(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);

    if (uuid == BLE_SVC_HID_CHR_UUID16_REPORT_MAP) {
        os_mbuf_append(ctxt->om, hid_report_map, sizeof(hid_report_map));
        return 0;
    }
    if (uuid == BLE_SVC_HID_CHR_UUID16_HID_INFO) {
        const uint8_t hid_info[] = {0x11, 0x01, 0x00, 0x01}; /* v1.11, RemoteWake enabled */
        os_mbuf_append(ctxt->om, hid_info, sizeof(hid_info));
        return 0;
    }
    if (uuid == 0x2a29) {
        os_mbuf_append(ctxt->om, "OnionLabs", 9);
        return 0;
    }
    if (uuid == BLE_SVC_HID_CHR_UUID16_PROTOCOL_MODE) {
        uint8_t mode = 1; /* 1 = Report Mode */
        os_mbuf_append(ctxt->om, &mode, sizeof(mode));
        return 0;
    }
    if (uuid == 0x2908) {
        uint8_t desc[] = {0x01, 0x01}; /* Report ID 1, Type Input */
        os_mbuf_append(ctxt->om, desc, sizeof(desc));
        return 0;
    }
    return 0;
}

/**
 * @brief Registers services and locates characteristic handles.
 */
void gatt_svr_init(void) {
    ble_gatts_count_cfg(gatt_svr_svcs);
    ble_gatts_add_svcs(gatt_svr_svcs);

    ble_gatts_find_chr(BLE_UUID16_DECLARE(BLE_SVC_HID_UUID16), 
                        BLE_UUID16_DECLARE(BLE_SVC_HID_CHR_UUID16_REPORT), 
                        NULL, &report_handle);
    ESP_LOGI(TAG, "HID Report Handle initialized: %d", report_handle);
}

void ble_app_on_sync(void) {
    ble_hs_id_infer_auto(0, &addr_type);
    ble_app_advertise();
}

void ble_app_on_reset(int reason) {
    ESP_LOGE(TAG, "BLE Host reset occurred. Reason: %d", reason);
}

void ble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE Host Task operational.");
    nimble_port_run(); 
    ESP_LOGE(TAG, "NimBLE port run terminated unexpectedly!");
    vTaskDelete(NULL);
}

/**
 * FreeRTOS Task: onion_comms_task
 * Handles serial commands from the PC application and streams real-time sensor telemetry.
 * * Protocol Support:
 * - Inbound: "CONNECT", "DISCONNECT", "SET:ch,thr,key"
 * - Outbound: "CFG:ch,thr,key", "RAW:v0,v1,...,v15"
 */
void onion_comms_task(void *pvParameters) {
    char line[128];      // Buffer to accumulate characters from serial
    int line_ptr = 0;    // Current write position in the buffer

    while(1) {
        /* --- 1. COMMAND PROCESSING (Inbound) --- */
        // Non-blocking read from stdin (Serial interface)
        int c = fgetc(stdin); 
        
        if (c != EOF) {
            // Check for line terminators or buffer overflow
            if (c == '\n' || c == '\r' || line_ptr >= sizeof(line) - 1) {
                line[line_ptr] = '\0'; // Null-terminate the string
                
                if (line_ptr > 0) {
                    // HANDSHAKE: PC app requested telemetry start
                    if (strcmp(line, "CONNECT") == 0) {
                        is_app_connected = true;
                        // Synchronize full configuration state back to PC immediately
                        for(int i = 0; i < 16; i++) {
                            printf("CFG:%d,%d,%d\n", i, local_lut_ptr[i].threshold, local_lut_ptr[i].keycode);
                        }
                    } 
                    // TERMINATION: PC app requested telemetry stop
                    else if (strcmp(line, "DISCONNECT") == 0) {
                        is_app_connected = false;
                    } 
                    // CONFIGURATION UPDATE: Received new parameters for a specific sensor
                    else if (strncmp(line, "SET:", 4) == 0) {
                        int ch, thr, key;
                        // Parse format: SET:channel,threshold,hid_keycode
                        if (sscanf(line, "SET:%d,%d,%d", &ch, &thr, &key) == 3) {
                            if (ch >= 0 && ch < 16) {
                                local_lut_ptr[ch].threshold = thr;
                                local_lut_ptr[ch].keycode = (uint8_t)key;
                                
                                // Persist changes to NVS (Non-Volatile Storage)
                                onion_config_save();
                            }
                        }
                    }
                }
                line_ptr = 0; // Reset buffer pointer after processing every line
            } else {
                // Character is part of a command, add to buffer
                line[line_ptr++] = (char)c;
            }
        }

        /* --- 2. TELEMETRY STREAMING (Outbound) --- */
        // Send sensor data only if a handshake has been established
        if (is_app_connected) {
            printf("RAW:");
            for (int i = 0; i < 16; i++) {
                // Output raw ADC values separated by commas
                printf("%u%s", last_raw_values[i], (i == 15) ? "" : ",");
            }
            printf("\n");
            
            // Ensure data is pushed through the serial bridge immediately
            fflush(stdout); 
        }

        /**
         * Task pacing: 50ms (20Hz)
         * Balancing UI responsiveness on PC vs CPU load on ESP32.
         */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


/**
 * @brief Complete BLE and HID stack initialization.
 */
int onion_ble_init(void){  
    vTaskDelay(pdMS_TO_TICKS(2000));
    /* Initialize Bluetooth Stack (NimBLE) */
    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_store_config_init();

    
    /* Initialize our custom GATT server (HID Keyboard) */
    gatt_svr_init();
    
    /* Set device name and assign sync callback */
    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.reset_cb = ble_app_on_reset;
    
    /* Pairing and Bonding config */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1; 
    ble_hs_cfg.sm_mitm = 0;    
    ble_hs_cfg.sm_sc = 0;      
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    /* Start the NimBLE host task */
    xTaskCreate(ble_host_task, "nimble_host", 4096, NULL, 5, NULL);

    local_lut_ptr = onion_lut;
    telemetry_source = last_raw_values;
    xTaskCreate(onion_comms_task, "telemetry_task", 4096, NULL, 5, NULL);

    return 0;
}