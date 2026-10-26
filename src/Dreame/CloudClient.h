#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Dreame/Types.h"
#include "Net/HttpsClient.h"

// Talks to the Dreame/MOVA cloud the way the MovaHome/DreameHome app does, following the
// Tasshack/dreame-vacuum integration's protocol.py. Uses a normal TLS connection; the
// integration's custom TLS client hello is not reproduced.
namespace ClankerCleaner::Dreame
{
    enum class AccountType
    {
        Mova,
        Dreame
    };

    struct CloudOptions
    {
        AccountType Account = AccountType::Mova;
        std::string Username;
        std::string Password;
        // Server region as in the app, e.g. "us", "eu", "cn".
        std::string Country = "us";
        // Device ID or name to use; empty picks the first robot vacuum on the account.
        std::string Device;
        std::chrono::seconds Timeout{10};
    };

    struct DeviceInfo
    {
        std::string Id;
        std::string Name;
        std::string Model;
        // The device's MQTT host, e.g. "10000.mt.us.iot.mova-tech.com:19974"; its first label
        // picks the command endpoint.
        std::string BindDomain;
    };

    class CloudError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // The cloud rejected the username, password or session. Retrying quickly risks
    // locking the account.
    class AuthenticationError : public CloudError
    {
    public:
        using CloudError::CloudError;
    };

    // The app's identity for one account type (host, app version, client credentials).
    struct AccountProfile;

    // Not thread-safe: use it from one thread (DockMonitor's).
    class CloudClient
    {
    public:
        explicit CloudClient(CloudOptions options);
        ~CloudClient();

        CloudClient(const CloudClient&) = delete;
        CloudClient& operator=(const CloudClient&) = delete;

        // Signs in with the refresh token when there is one, otherwise the password.
        void Login();
        bool IsLoggedIn() const { return !accessToken.empty(); }

        std::vector<DeviceInfo> ListDevices();

        // Picks CloudOptions::Device (matched by ID or name, ignoring case), or the first
        // robot vacuum. Throws CloudError if there's no match.
        const DeviceInfo& SelectDevice();
        const std::optional<DeviceInfo>& GetSelectedDevice() const { return selectedDevice; }

        // Reads properties from the selected device. Ones the device doesn't report, or that
        // aren't whole numbers, are left out of the result.
        std::map<PropertyId, std::int64_t> GetProperties(const std::vector<PropertyId>& properties);

    private:
        // Logs in when there's no session or the access token expires within ten minutes.
        void EnsureSession();
        // Returns false when a refresh token is rejected, so the caller can fall back to the password.
        bool RequestToken(const std::string& formBody, bool usingRefreshToken);
        Net::HttpHeaders AppHeaders(bool includeSession) const;
        // POSTs signed JSON (or no body when parameters is null); signs in again once on a 401.
        nlohmann::json ApiCall(const std::string& path, const nlohmann::json* parameters);

        CloudOptions options;
        const AccountProfile& profile;
        Net::HttpsClient http;
        // Stands in for the app's browser-fingerprint hash; random per run, as in the integration.
        std::string visitorHash;

        std::string accessToken;
        std::string refreshToken;
        std::chrono::steady_clock::time_point accessTokenExpiry{};
        std::string tenantId;
        std::string region;
        std::string language;
        std::string countryCode;

        std::optional<DeviceInfo> selectedDevice;
        std::int64_t requestId;
    };
}
