#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "string.h"

/**
 * embedded binary file: wifi.txt
 * format: ssid,pwd
 */
extern const uint8_t test_wifi_cfg_start_[] asm("_binary_wifi_cfg_start");
extern const uint8_t test_wifi_cfg_end_[] asm("_binary_wifi_cfg_end");

#define TEST_TAG "TEST"

static const uint32_t EVENT_GROUP_STA_CONNECTED = (1 << 0);
static const uint32_t EVENT_GROUP_STA_CONNECTED_GOT_IP = (1 << 1);
static const uint32_t EVENT_GROUP_STA_DISCONNECTED = (1 << 2);

typedef struct {
    EventGroupHandle_t evtGroup;
    esp_netif_t *pSta;
    char wifiSsid[31];
    char wifiPwd[51];
} Global_t;
EXT_RAM_ATTR static Global_t GV;

// print MACRO
#define WFM1_STRINGIFY(x) WFM1_STRINGIFY2(x)
#define WFM1_STRINGIFY2(x) #x
static void printMacro(const char *s1, const char *s2) {
    int isSame = (s2 && !strcmp(s1, s2));
    ESP_LOGI(TEST_TAG, "    %-50s %s", s1, isSame ? "(not defined)" : s2);
}

static void printMenuconfig(void) {
    ESP_LOGI(TEST_TAG, "=================================================");
    ESP_LOGI(TEST_TAG, "esp-idf=%s", esp_get_idf_version());
    printMacro("CONFIG_ESP32_SPIRAM_SUPPORT", WFM1_STRINGIFY(CONFIG_ESP32_SPIRAM_SUPPORT));
    printMacro("CONFIG_SPIRAM", WFM1_STRINGIFY(CONFIG_SPIRAM));
    printMacro("CONFIG_SPIRAM_BOOT_INIT", WFM1_STRINGIFY(CONFIG_SPIRAM_BOOT_INIT));
    printMacro("CONFIG_SPIRAM_USE_MALLOC", WFM1_STRINGIFY(CONFIG_SPIRAM_USE_MALLOC));
    printMacro("CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL", WFM1_STRINGIFY(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL));
    printMacro("CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL", WFM1_STRINGIFY(CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL));
    printMacro("CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY", WFM1_STRINGIFY(CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY));
    printMacro("CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", WFM1_STRINGIFY(CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY));

    printMacro("CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST", WFM1_STRINGIFY(CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST));
    printMacro("CONFIG_BT_BLE_DYNAMIC_ENV_MEMORY", WFM1_STRINGIFY(CONFIG_BT_BLE_DYNAMIC_ENV_MEMORY));
    ESP_LOGI(TEST_TAG, "=================================================");
}

/**
 * @brief WiFi callback event handler
 */
static void wifiEventCB(void *arg, esp_event_base_t base, int32_t event, void *eventData) {
    ESP_LOGD(TEST_TAG, "%s, base=%p, event=%d", __func__, base, event);

    //////////////////////////////////////////////////////////////
    // Wifi event
    if (base == WIFI_EVENT) {
        /////////////////////////////////////////////////////////////////
        // STA wifi events
        /////////////////////////////////////////////////////////////////
        // STA started and is ready to connect to AP
        if (event == WIFI_EVENT_STA_START) {
            ESP_LOGI(TEST_TAG, "(STA) started");
        }

        else if (event == WIFI_EVENT_STA_STOP) {
            ESP_LOGI(TEST_TAG, "(STA) stopped");
        }

        // STA connected
        else if (event == WIFI_EVENT_STA_CONNECTED) {
            wifi_event_sta_connected_t *pConn = (wifi_event_sta_connected_t *)eventData;
            ESP_LOGI(TEST_TAG, "(STA) connected, ssid=%s, channel=%d", (char *)pConn->ssid, pConn->channel);
            xEventGroupSetBits(GV.evtGroup, EVENT_GROUP_STA_CONNECTED);
        }

        // STA disconnected
        else if (event == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI(TEST_TAG, "(STA) disconnected");
            xEventGroupSetBits(GV.evtGroup, EVENT_GROUP_STA_DISCONNECTED);
        }

        else {
            ESP_LOGE(TEST_TAG, "unknown Wi-Fi event");
        }
    }

    //////////////////////////////////////////////////////////////
    // IP event
    else if (base == IP_EVENT) {
        /////////////////////////////////////////////////////////////////
        // STA IP events
        /////////////////////////////////////////////////////////////////
        // (STA) IP address assigned
        if (event == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *pIp = (ip_event_got_ip_t *)eventData;
            ESP_LOGI(TEST_TAG, "(STA) IP address: " IPSTR "", IP2STR(&pIp->ip_info.ip));
            xEventGroupSetBits(GV.evtGroup, EVENT_GROUP_STA_CONNECTED_GOT_IP);
        }

        else {
            ESP_LOGE(TEST_TAG, "unknown IP event");
        }
    } else {
        ESP_LOGI(TEST_TAG, "unknown event");
    }
}

/**
 * @brief parse wifi.cfg and get Wi-Fi ssid and pwd from wifi.cfg
 */
static const char *getWiFiSSIDPwd(void) {
    static const char *errStr = NULL;
    char buff[101] = "";
    do {
        size_t cfgSize = test_wifi_cfg_end_ - test_wifi_cfg_start_;
        size_t copySize = cfgSize > sizeof(buff) ? sizeof(buff) : cfgSize;
        snprintf(buff, copySize, (char *)test_wifi_cfg_start_);
        int n = sscanf(buff, "%30[^,],%50[^\n]\n", GV.wifiSsid, GV.wifiPwd);
        if (n != 2) {
            errStr = "invalid wifi.cfg. WiFi will not start";
            break;
        }
        ESP_LOGI(TEST_TAG, "wifi.cfg, ssid='%s', pwd='%s'", GV.wifiSsid, GV.wifiPwd);

    } while (0);
    return errStr;
}

/**
 * @brief Init Wifi and connect to AP
 * @return If OK, return NULL. If error, return err string.
 */
static const char *init_wifi(void) {
    static const char *errStr = NULL;
    do {
        // reset global variable
        memset(&GV, 0, sizeof(GV));

        // parse Wi-Fi ssid and password from wifi.cfg
        if ((errStr = getWiFiSSIDPwd())) {
            break;
        }

        // init wifi
        esp_err_t espRet;
        if (esp_netif_init() != ESP_OK) {
            errStr = "esp_netif_init() failed";
            break;
        }

        if (esp_event_loop_create_default() != ESP_OK) {
            errStr = "esp_event_loop_create_default() failed";
            break;
        }

        if ((GV.pSta = esp_netif_create_default_wifi_sta()) == NULL) {
            errStr = "esp_netif_create_default_wifi_sta() failed";
            break;
        }

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        if (esp_wifi_init(&cfg) != ESP_OK) {
            errStr = "esp_wifi_init() failed";
            break;
        }

        // register events
        GV.evtGroup = xEventGroupCreate();
        espRet = 0;
        espRet += esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventCB, NULL);
        espRet += esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifiEventCB, NULL);
        if (espRet != ESP_OK) {
            errStr = "esp_event_handler_register() failed";
            break;
        }

        // set STA mode
        if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
            errStr = "esp_wifi_set_mode(WIFI_MODE_STA) failed";
            break;
        }

        // start Wi-Fi
        if (esp_wifi_start() != ESP_OK) {
            errStr = "esp_wifi_start() failed";
            break;
        }

    } while (0);
    return errStr;
}

/**
 * @brief Start WiFi STA and connect to SSID/pwd
 */
static const char *connect_wifi(void) {
    static const char *errStr = NULL;
    do {
        // connect to AP
        wifi_config_t wifiCfg = {};
        wifiCfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; // weakest security to connect
        // wifiCfg.sta.threshold.authmode = WIFI_AUTH_WEP;
        // wifiCfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        wifiCfg.sta.pmf_cfg.capable = true;
        wifiCfg.sta.pmf_cfg.required = false;
        snprintf((char *)wifiCfg.sta.ssid, sizeof(wifiCfg.sta.ssid), "%s", GV.wifiSsid);
        snprintf((char *)wifiCfg.sta.password, sizeof(wifiCfg.sta.password), GV.wifiPwd);
        if (esp_wifi_set_config(WIFI_IF_STA, &wifiCfg) != ESP_OK) {
            errStr = "esp_wifi_set_config() failed";
            break;
        }
        ESP_LOGI(TEST_TAG, "WiFi connecting, ssid=%s, pwd=%s", wifiCfg.sta.ssid, wifiCfg.sta.password);
        if (esp_wifi_connect() != ESP_OK) {
            errStr = "esp_wifi_connect() failed";
            break;
        }

    } while (0);
    return errStr;
}

void init_test(void) {
    const char *errStr = NULL;
    printMenuconfig();
    do {
        // init wifi
        if ((errStr = init_wifi()))
            break;

        // connect to AP
        if ((errStr = connect_wifi()))
            break;

        // wait for connected (Got IP)
        ESP_LOGI(TEST_TAG, "waiting connecting to AP...");
        EventBits_t bits = xEventGroupWaitBits(GV.evtGroup,
                                               EVENT_GROUP_STA_CONNECTED_GOT_IP | EVENT_GROUP_STA_DISCONNECTED,
                                               pdFALSE,
                                               pdFALSE,
                                               portMAX_DELAY);
        if (bits & EVENT_GROUP_STA_CONNECTED_GOT_IP) {
            ESP_LOGI(TEST_TAG, "WiFi connected to AP");
        } else {
            ESP_LOGE(TEST_TAG, "Failed to connect to AP");
            ESP_LOGI(TEST_TAG, "Stopping Wi-Fi...");
            if (esp_wifi_stop() != ESP_OK) {
                errStr = "esp_wifi_stop() failed";
                break;
            }
        }

    } while (0);

    if (errStr)
        ESP_LOGE(TEST_TAG, "err=%s", errStr);
}
