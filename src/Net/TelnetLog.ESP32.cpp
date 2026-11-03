// One low-priority task does everything: it wakes when something is logged (or every
// AcceptInterval to check for new connections), accepts clients, and sends each one what it
// hasn't seen yet. A client that can't keep up for SendTimeout is dropped.

#include "Net/TelnetLog.ESP32.h"
#include "Logging/LogCapture.ESP32.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <string_view>

#include <esp_log.h>
#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>

namespace ClankerCleaner::Net::TelnetLog
{
    namespace
    {
        namespace Capture = Logging::Capture;

        constexpr const char* Tag = "telnet_log";
        constexpr std::uint16_t Port = 23;
        constexpr std::size_t MaxClients = 2;
        constexpr TickType_t AcceptInterval = pdMS_TO_TICKS(250);
        constexpr timeval SendTimeout{2, 0};
        constexpr std::uint32_t StackSize = 4096;
        constexpr UBaseType_t Priority = tskIDLE_PRIORITY + 2;

        struct Client
        {
            int Socket = -1;
            Capture::Position Position = 0;
        };

        void Close(Client& client)
        {
            close(client.Socket);
            client.Socket = -1;
        }

        bool SendAll(int socket, const char* data, std::size_t size)
        {
            while(size > 0)
            {
                const ssize_t sent = send(socket, data, size, 0);
                if(sent <= 0)
                {
                    return false;
                }
                data += sent;
                size -= static_cast<std::size_t>(sent);
            }
            return true;
        }

        // Telnet terminals want CRLF line ends; the log has bare LFs.
        bool SendText(int socket, const char* data, std::size_t size)
        {
            std::array<char, 1024> out;
            std::size_t used = 0;
            for(std::size_t i = 0; i < size; ++i)
            {
                if(used + 2 > out.size())
                {
                    if(!SendAll(socket, out.data(), used))
                    {
                        return false;
                    }
                    used = 0;
                }
                if(data[i] == '\n')
                {
                    out[used++] = '\r';
                }
                out[used++] = data[i];
            }
            return SendAll(socket, out.data(), used);
        }

        bool SendText(int socket, std::string_view text)
        {
            return SendText(socket, text.data(), text.size());
        }

        int Listen()
        {
            const int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if(listener < 0)
            {
                return -1;
            }
            const int reuse = 1;
            setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(Port);
            address.sin_addr.s_addr = htonl(INADDR_ANY);
            if(bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
               listen(listener, static_cast<int>(MaxClients)) != 0)
            {
                close(listener);
                return -1;
            }
            fcntl(listener, F_SETFL, fcntl(listener, F_GETFL, 0) | O_NONBLOCK);
            return listener;
        }

        void Accept(int listener, std::array<Client, MaxClients>& clients)
        {
            for(int socket; (socket = accept(listener, nullptr, nullptr)) >= 0;)
            {
                auto client = std::find_if(clients.begin(), clients.end(), [](const Client& c) { return c.Socket < 0; });
                if(client == clients.end())
                {
                    SendText(socket, "ClankerCleaner log: too many viewers already connected\n");
                    close(socket);
                    continue;
                }
                setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &SendTimeout, sizeof(SendTimeout));
                const int noDelay = 1;
                setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
                client->Socket = socket;
                client->Position = Capture::Oldest();
                if(!SendText(socket, "ClankerCleaner log; recent lines first\n"))
                {
                    Close(*client);
                }
            }
        }

        // False once the client has gone.
        bool Serve(Client& client)
        {
            // Input (including telnet option negotiation) is read and thrown away.
            std::array<char, 64> input;
            const ssize_t received = recv(client.Socket, input.data(), input.size(), MSG_DONTWAIT);
            if(received == 0 || (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
            {
                return false;
            }

            std::array<char, 512> chunk;
            for(;;)
            {
                bool dropped = false;
                const std::size_t count = Capture::Read(client.Position, chunk.data(), chunk.size(), dropped);
                if(dropped && !SendText(client.Socket, "\n[... older lines dropped; this viewer fell behind ...]\n"))
                {
                    return false;
                }
                if(count == 0)
                {
                    return true;
                }
                if(!SendText(client.Socket, chunk.data(), count))
                {
                    return false;
                }
            }
        }

        void Run(void* /*argument*/)
        {
            const int listener = Listen();
            if(listener < 0 || !Capture::AddReader(xTaskGetCurrentTaskHandle()))
            {
                ESP_LOGE(Tag, "can't serve the log on port %u", Port);
                if(listener >= 0)
                {
                    close(listener);
                }
                vTaskDelete(nullptr);
                return;
            }
            ESP_LOGI(Tag, "serving the log on port %u", Port);

            std::array<Client, MaxClients> clients;
            for(;;)
            {
                ulTaskNotifyTake(pdTRUE, AcceptInterval);
                Accept(listener, clients);
                for(Client& client : clients)
                {
                    if(client.Socket >= 0 && !Serve(client))
                    {
                        Close(client);
                    }
                }
            }
        }
    }

    void Start()
    {
        if(xTaskCreate(Run, "telnet_log", StackSize, nullptr, Priority, nullptr) != pdPASS)
        {
            ESP_LOGE(Tag, "can't start the log server task");
        }
    }
}
