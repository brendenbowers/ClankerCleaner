// ESP32 network startup: WiFi on the board, or the OpenCores Ethernet that Espressif's QEMU
// emulates (the QEMU build sets CONFIG_ETH_USE_OPENETH). This runs before the program sets
// up its logger, so it logs through ESP-IDF's.

#include "Net/Network.ESP32.h"
#include "Config/Secrets.ESP32.h"

#include <algorithm>
#include <string_view>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <nvs_flash.h>

#if CONFIG_ETH_USE_OPENETH
#include <esp_eth.h>
#include <esp_eth_mac_openeth.h>
#else
#include <esp_wifi.h>
#endif

namespace ClankerCleaner::Net::Network
{
    namespace
    {
        constexpr const char* Tag = "network";
        constexpr TickType_t AddressTimeout = pdMS_TO_TICKS(30000);
        constexpr TickType_t ClockSyncTimeout = pdMS_TO_TICKS(30000);
        constexpr const char* TimeServer = "pool.ntp.org";
        constexpr EventBits_t GotAddressBit = BIT0;

        EventGroupHandle_t events = nullptr;

        bool Check(esp_err_t result, const char* action)
        {
            if(result != ESP_OK)
            {
                ESP_LOGE(Tag, "%s failed: %s", action, esp_err_to_name(result));
                return false;
            }
            return true;
        }

        void OnGotAddress(void* /*argument*/, esp_event_base_t /*base*/, int32_t /*id*/, void* data)
        {
            const auto* event = static_cast<const ip_event_got_ip_t*>(data);
            ESP_LOGI(Tag, "got address " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(events, GotAddressBit);
        }

        bool StartStorage()
        {
            // WiFi keeps its calibration data in NVS. A partition from another firmware or
            // ESP-IDF version is wiped rather than refusing to start.
            esp_err_t result = nvs_flash_init();
            if(result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                Check(nvs_flash_erase(), "erasing NVS");
                result = nvs_flash_init();
            }
            return Check(result, "starting NVS");
        }

#if CONFIG_ETH_USE_OPENETH
        bool StartLink()
        {
            esp_netif_config_t netifConfig = ESP_NETIF_DEFAULT_ETH();
            esp_netif_t* netif = esp_netif_new(&netifConfig);

            eth_mac_config_t macConfig = ETH_MAC_DEFAULT_CONFIG();
            eth_phy_config_t phyConfig = ETH_PHY_DEFAULT_CONFIG();
            // QEMU's emulated PHY has no reset line and its link is up at once.
            phyConfig.reset_gpio_num = -1;
            phyConfig.autonego_timeout_ms = 100;
            esp_eth_mac_t* mac = esp_eth_mac_new_openeth(&macConfig);
            esp_eth_phy_t* phy = esp_eth_phy_new_dp83848(&phyConfig);

            esp_eth_config_t ethConfig = ETH_DEFAULT_CONFIG(mac, phy);
            esp_eth_handle_t handle = nullptr;
            if(!netif || !Check(esp_eth_driver_install(&ethConfig, &handle), "installing the Ethernet driver") ||
               !Check(esp_netif_attach(netif, esp_eth_new_netif_glue(handle)), "attaching Ethernet") ||
               !Check(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, OnGotAddress, nullptr), "watching for an address"))
            {
                return false;
            }
            ESP_LOGI(Tag, "starting the emulated Ethernet");
            return Check(esp_eth_start(handle), "starting Ethernet");
        }
#else
        void OnWifiEvent(void* /*argument*/, esp_event_base_t /*base*/, int32_t id, void* /*data*/)
        {
            if(id == WIFI_EVENT_STA_DISCONNECTED)
            {
                // Keep trying; the first attempt's failure is reported by Start's timeout.
                xEventGroupClearBits(events, GotAddressBit);
                ESP_LOGW(Tag, "WiFi disconnected from \"%s\"; reconnecting", Secrets::WifiSsid);
            }
            esp_wifi_connect();
        }

        bool StartLink()
        {
            const std::string_view ssid = Secrets::WifiSsid;
            const std::string_view password = Secrets::WifiPassword;
            if(ssid.empty())
            {
                ESP_LOGW(Tag, "no WiFi network set in esp32/Secrets.h; running offline");
                return false;
            }

            wifi_config_t wifiConfig{};
            if(ssid.size() > sizeof(wifiConfig.sta.ssid) || password.size() >= sizeof(wifiConfig.sta.password))
            {
                ESP_LOGE(Tag, "the WiFi network name or password in esp32/Secrets.h is too long");
                return false;
            }
            std::copy(ssid.begin(), ssid.end(), wifiConfig.sta.ssid);
            std::copy(password.begin(), password.end(), wifiConfig.sta.password);
            wifiConfig.sta.threshold.authmode = password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

            esp_netif_create_default_wifi_sta();
            const wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
            if(!Check(esp_wifi_init(&initConfig), "starting WiFi") ||
               !Check(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, OnWifiEvent, nullptr), "watching WiFi") ||
               !Check(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, OnWifiEvent, nullptr), "watching WiFi") ||
               !Check(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, OnGotAddress, nullptr), "watching for an address") ||
               !Check(esp_wifi_set_mode(WIFI_MODE_STA), "setting the WiFi mode") ||
               !Check(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig), "setting the WiFi network"))
            {
                return false;
            }
            ESP_LOGI(Tag, "joining WiFi network \"%s\"", Secrets::WifiSsid);
            return Check(esp_wifi_start(), "starting WiFi");
        }
#endif
    }

    bool Start()
    {
        // The stack starts even when the link can't, so sockets opened later fail with an
        // error instead of tripping lwIP's asserts.
        events = xEventGroupCreate();
        if(!events || !Check(esp_netif_init(), "starting the network stack") ||
           !Check(esp_event_loop_create_default(), "starting the event loop") || !StartStorage() || !StartLink())
        {
            return false;
        }

        if(!(xEventGroupWaitBits(events, GotAddressBit, pdFALSE, pdTRUE, AddressTimeout) & GotAddressBit))
        {
            ESP_LOGE(Tag, "no network address after %lu s; running offline until it connects",
                     static_cast<unsigned long>(pdTICKS_TO_MS(AddressTimeout) / 1000));
            return false;
        }

        // SNTP keeps the clock synced from here on.
        const esp_sntp_config_t sntpConfig = ESP_NETIF_SNTP_DEFAULT_CONFIG(TimeServer);
        if(!Check(esp_netif_sntp_init(&sntpConfig), "starting SNTP"))
        {
            return false;
        }
        if(esp_netif_sntp_sync_wait(ClockSyncTimeout) != ESP_OK)
        {
            ESP_LOGE(Tag, "the clock didn't sync from %s; certificate checks fail until it does", TimeServer);
            return false;
        }
        ESP_LOGI(Tag, "clock synced from %s", TimeServer);
        return true;
    }
}
