// The upload is written to the spare slot as it arrives, in Chunk-sized pieces, so the image
// never has to fit in RAM. esp_ota_end checks the image (header and SHA-256) before it's
// made the boot slot.
//
// While an update is written the relays are switched off and their pins held there, so the
// state machine, which keeps running, can't start a pump or valve until the restart. A
// software restart keeps the hold, so Start releases it before the pins are configured.

#include "Net/OtaUpdate.ESP32.h"
#include "Config/Config.h"
#include "Config/Secrets.ESP32.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string_view>

#include <driver/gpio.h>
#include <esp_app_desc.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ClankerCleaner::Net::OtaUpdate
{
    namespace
    {
        constexpr const char* Tag = "ota";
        constexpr std::uint16_t Port = 80;
        constexpr std::size_t Chunk = 4096;
        // Enough to answer the PC before the restart cuts the connection.
        constexpr TickType_t RestartDelay = pdMS_TO_TICKS(500);
        // A receive that times out is retried this many times before the upload is dropped.
        constexpr int MaxReceiveTimeouts = 5;

        constexpr TickType_t ConfirmAfter = pdMS_TO_TICKS(60 * 1000);
        constexpr TickType_t ConfirmRetry = pdMS_TO_TICKS(30 * 1000);
        constexpr int ConfirmAttempts = 9;

        std::array<gpio_num_t, 4> RelayPins(const Config::Settings& settings)
        {
            return {static_cast<gpio_num_t>(settings.Gpio.PumpRelayPin),
                    static_cast<gpio_num_t>(settings.Gpio.DirtyTankRinseRelayPin),
                    static_cast<gpio_num_t>(settings.Gpio.CleanTankFillRelayPin),
                    static_cast<gpio_num_t>(settings.Gpio.CleanerDosingRelayPin)};
        }

        void HoldRelaysOff(bool hold)
        {
            const Config::Settings settings = Config::Load({}, false);
            for(const gpio_num_t pin : RelayPins(settings))
            {
                if(hold)
                {
                    gpio_set_level(pin, settings.Gpio.RelayActiveLow ? 1 : 0);
                    gpio_hold_en(pin);
                }
                else
                {
                    gpio_hold_dis(pin);
                }
            }
        }

        // Compares every byte, so the time taken doesn't show how much of a guess was right.
        bool TokenMatches(std::string_view given, std::string_view expected)
        {
            if(expected.empty())
            {
                return false;
            }
            std::size_t difference = given.size() ^ expected.size();
            for(std::size_t i = 0; i < given.size(); ++i)
            {
                difference |= static_cast<unsigned char>(given[i]) ^ static_cast<unsigned char>(expected[i % expected.size()]);
            }
            return difference == 0;
        }

        bool Authorized(httpd_req_t* request)
        {
            std::array<char, 128> header{};
            if(httpd_req_get_hdr_value_str(request, "Authorization", header.data(), header.size()) != ESP_OK)
            {
                return false;
            }
            constexpr std::string_view Scheme = "Bearer ";
            const std::string_view value(header.data());
            return value.starts_with(Scheme) && TokenMatches(value.substr(Scheme.size()), Secrets::OtaToken);
        }

        esp_err_t Fail(httpd_req_t* request, httpd_err_code_t code, const char* message)
        {
            ESP_LOGW(Tag, "update refused: %s", message);
            return httpd_resp_send_err(request, code, message);
        }

        esp_err_t HandleVersion(httpd_req_t* request)
        {
            const esp_app_desc_t* app = esp_app_get_description();
            const esp_partition_t* running = esp_ota_get_running_partition();
            std::array<char, 160> body{};
            std::snprintf(body.data(), body.size(), "%s %s, built %s %s, running from %s\n", app->project_name,
                          app->version, app->date, app->time, running->label);
            httpd_resp_set_type(request, "text/plain");
            return httpd_resp_sendstr(request, body.data());
        }

        esp_err_t HandleUpdate(httpd_req_t* request)
        {
            if(!Authorized(request))
            {
                return Fail(request, HTTPD_401_UNAUTHORIZED, "wrong or missing token");
            }

            const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
            if(!target)
            {
                return Fail(request, HTTPD_500_INTERNAL_SERVER_ERROR, "no spare app slot; flash the OTA partition table over USB");
            }
            const std::size_t size = request->content_len;
            if(size == 0 || size > target->size)
            {
                return Fail(request, HTTPD_400_BAD_REQUEST, "image is empty or too big for the app slot");
            }

            const auto buffer = std::make_unique<char[]>(Chunk);
            esp_ota_handle_t ota = 0;
            if(esp_ota_begin(target, size, &ota) != ESP_OK)
            {
                return Fail(request, HTTPD_500_INTERNAL_SERVER_ERROR, "couldn't start writing the app slot");
            }
            ESP_LOGI(Tag, "receiving %u bytes into %s; relays held off", static_cast<unsigned>(size), target->label);
            HoldRelaysOff(true);

            std::size_t received = 0;
            int timeouts = 0;
            while(received < size)
            {
                const int count = httpd_req_recv(request, buffer.get(), std::min(Chunk, size - received));
                if(count == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts <= MaxReceiveTimeouts)
                {
                    continue;
                }
                if(count <= 0 || esp_ota_write(ota, buffer.get(), count) != ESP_OK)
                {
                    esp_ota_abort(ota);
                    HoldRelaysOff(false);
                    return Fail(request, HTTPD_500_INTERNAL_SERVER_ERROR, "upload broke off or couldn't be written");
                }
                received += count;
                timeouts = 0;
            }

            if(esp_ota_end(ota) != ESP_OK)
            {
                HoldRelaysOff(false);
                return Fail(request, HTTPD_400_BAD_REQUEST, "not a valid firmware image");
            }
            if(esp_ota_set_boot_partition(target) != ESP_OK)
            {
                HoldRelaysOff(false);
                return Fail(request, HTTPD_500_INTERNAL_SERVER_ERROR, "couldn't switch the boot slot");
            }

            ESP_LOGI(Tag, "update written to %s; restarting", target->label);
            httpd_resp_sendstr(request, "updated, restarting\n");
            vTaskDelay(RestartDelay);
            esp_restart();
            return ESP_OK;
        }

        bool HasAddress()
        {
            esp_netif_t* netif = esp_netif_get_default_netif();
            esp_netif_ip_info_t info{};
            return netif && esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0;
        }

        // Confirms a new update once it has run a while with an address, so the update server
        // can reach it again; otherwise restarts, and the bootloader rolls back.
        void ConfirmTask(void* /*argument*/)
        {
            vTaskDelay(ConfirmAfter);
            for(int attempt = 0; attempt < ConfirmAttempts && !HasAddress(); ++attempt)
            {
                vTaskDelay(ConfirmRetry);
            }

            if(HasAddress())
            {
                esp_ota_mark_app_valid_cancel_rollback();
                ESP_LOGI(Tag, "update confirmed");
            }
            else
            {
                ESP_LOGE(Tag, "no network address since the update; restarting into the previous firmware");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
            vTaskDelete(nullptr);
        }

        void StartServer()
        {
            httpd_config_t config = HTTPD_DEFAULT_CONFIG();
            config.server_port = Port;
            config.stack_size = 6144;
            httpd_handle_t server = nullptr;
            if(httpd_start(&server, &config) != ESP_OK)
            {
                ESP_LOGE(Tag, "update server didn't start");
                return;
            }

            const httpd_uri_t update{.uri = "/update", .method = HTTP_POST, .handler = HandleUpdate, .user_ctx = nullptr};
            const httpd_uri_t version{.uri = "/version", .method = HTTP_GET, .handler = HandleVersion, .user_ctx = nullptr};
            httpd_register_uri_handler(server, &update);
            httpd_register_uri_handler(server, &version);
            ESP_LOGI(Tag, "taking updates on port %u", static_cast<unsigned>(Port));
        }
    }

    void Start()
    {
        HoldRelaysOff(false);

        esp_ota_img_states_t state{};
        if(esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            ESP_LOGI(Tag, "running a new update; it confirms itself once it has a network address");
            xTaskCreate(ConfirmTask, "ota_confirm", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr);
        }

        if(std::string_view(Secrets::OtaToken).empty())
        {
            ESP_LOGW(Tag, "no OtaToken in esp32/Secrets.h; over-the-air updates are off");
            return;
        }
        StartServer();
    }
}
