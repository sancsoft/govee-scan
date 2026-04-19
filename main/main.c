#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#define GOVEE_MANUF_ID 0xEC88
#define TAG "GOVEE"

#define WIFI_SSID "sancsoft"
#define WIFI_PASSWORD "aoxomoxoa"

static float g_temp = -100.0f;
static float g_hum = -100.0f;
static char g_mac[18] = "Unknown";

static httpd_handle_t g_httpd = NULL;

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
                    g_temp = (float)val / 10000.0;
                    g_hum = (float)(val % 1000) / 10.0;
                    
                    snprintf(g_mac, sizeof(g_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                             param->scan_rst.bda[5], param->scan_rst.bda[4], param->scan_rst.bda[3],
                             param->scan_rst.bda[2], param->scan_rst.bda[1], param->scan_rst.bda[0]);
                    
                    ESP_LOGI(TAG, "Device: %s | Temp: %.2f°C | Hum: %.1f%% | Batt: %d%%", 
                             g_mac, g_temp, g_hum, mfg_data[6]);
                }
            }
        }
    }
}

static esp_err_t http_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    
    char response[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Govee Sensor</title>
    <style>
        body { font-family: sans-serif; display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 100vh; background: #1a1a2e; color: #fff; }
        .card { background: rgba(255,255,255,0.1); padding: 40px; border-radius: 20px; text-align: center; }
        .temp { font-size: 4rem; color: #4db6ac; margin: 20px 0; }
        .hum { font-size: 3rem; color: #81d4fa; }
        .label { color: #aaa; font-size: 1.2rem; }
        .mac { color: #666; font-family: monospace; font-size: 0.9rem; margin-top: 20px; }
    </style>
</head>
<body>
    <div class="card">
        <h1>Govee Sensor</h1>
        <div class="temp"><span id="t">--</span>°C</div>
        <div class="hum"><span id="h">--</span>%</div>
        <div class="label">Temperature</div>
        <div class="label">Humidity</div>
        <div class="mac" id="mac">Waiting...</div>
    </div>
    <script>
        setInterval(() => fetch('/api/sensor').then(r=>r.json()).then(d=>{ 
            if(d.temp>-99){ document.getElementById('t').textContent=d.temp.toFixed(1); document.getElementById('h').textContent=d.hum.toFixed(1); }
            if(d.mac) document.getElementById('mac').textContent='MAC: '+d.mac;
        }), 2000);
    </script>
</body>
</html>
)rawliteral";
    
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

static esp_err_t json_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    char json[128];
    snprintf(json, sizeof(json), "{\"temp\":%.1f,\"hum\":%.1f,\"mac\":\"%s\"}", g_temp, g_hum, g_mac);
    
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

static const httpd_uri_t http_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = http_handler,
    .user_ctx = NULL
};

static const httpd_uri_t json_uri = {
    .uri = "/api/sensor",
    .method = HTTP_GET,
    .handler = json_handler,
    .user_ctx = NULL
};

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
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&g_httpd, &config);
    httpd_register_uri_handler(g_httpd, &http_uri);
    httpd_register_uri_handler(g_httpd, &json_uri);
}
