#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#define GOVEE_MANUF_ID 0xEC88
#define TAG "GOVEE"

#define WIFI_SSID "sancsoft"
#define WIFI_PASSWORD "aoxomoxoa"

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected! IP: %s", esp_ip4addr_ntoa(&event->ip_info.ip, NULL, 0));
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    if (event == ESP_GAP_BLE_SCAN_RESULT_EVT) {
        if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            
            uint8_t *adv_data = param->scan_rst.ble_adv;
            int adv_data_len = param->scan_rst.adv_data_len;
            uint8_t *mfg_data = NULL;
            int mfg_data_len = 0;

            mfg_data = esp_ble_resolve_adv_data(adv_data, ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, (uint8_t *)&mfg_data_len);

            if (mfg_data && mfg_data_len >= 7) {
                uint16_t mfg_id = mfg_data[0] | (mfg_data[1] << 8);
                if (mfg_id == GOVEE_MANUF_ID) {
                    uint32_t val = (mfg_data[3] << 16) | (mfg_data[4] << 8) | mfg_data[5];
                    float temp = (float)val / 10000.0;
                    float hum = (float)(val % 1000) / 10.0;
                    
                    ESP_LOGI(TAG, "Device: " ESP_BD_ADDR_STR " | Temp: %.2f°C | Hum: %.1f%% | Batt: %d%%", 
                             ESP_BD_ADDR_HEX(param->scan_rst.bda), temp, hum, mfg_data[6]);
                }
            }
        }
    }
}

void app_main(void) {
    nvs_flash_init();
    
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    
    esp_event_loop_create_default();
    
    esp_netif_init();
    esp_netif_create_default_wifi_sta();
    
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL);
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    esp_wifi_set_config(WIFI_MODE_STA, &wifi_config);
    esp_wifi_connect();
    
    esp_ble_gap_register_callback(esp_gap_cb);
    
    static esp_ble_scan_params_t ble_scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x100,
        .scan_window = 0x50,
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE
    };
    esp_ble_gap_set_scan_params(&ble_scan_params);
    esp_ble_gap_start_scanning(10);
}
