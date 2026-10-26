#include "Dreame/CloudClient.h"

#include "Crypto/Crypto.h"
#include "Dreame/Signing.h"
#include "Logging/Logging.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <random>
#include <string_view>

namespace ClankerCleaner::Dreame
{
    using nlohmann::json;

    // Values the app sends, from the integration's string table.
    struct AccountProfile
    {
        std::string_view HostSuffix;
        std::string_view UserAgent;
        std::string_view AppVersion;
        std::string_view BasicAuthorization;
        // Signs requests and encrypts the region header.
        std::string_view SigningKey;
        std::string_view DefaultTenantId;
        bool SendsPasswordStyleHeader;
    };

    namespace
    {
        constexpr AccountProfile MovaProfile{".iot.mova-tech.com", "Dart/3.10 (dart:io)", "10605530",
                                             "bW92YV9hcHA6VjdLb0NoTFc4dkhBQ3FHYg==", "gigxlmqwZ]7oWZUF", "000002", true};
        constexpr AccountProfile DreameProfile{".iot.dreame.tech", "Dart/3.9 (dart:io)", "102060300",
                                               "ZHJlYW1lX2FwcHYxOkFQXmR2QHpAU1FZVnhOODg=", "EETjszu*XI5znHsI", "000000", false};

        constexpr int ApiPort = 13267;
        constexpr std::string_view PasswordSalt = "RAylYC%fmSKp7%Tq";
        constexpr std::chrono::minutes TokenRefreshMargin{10};
        constexpr std::size_t MaxBodyInErrors = 300;

        const AccountProfile& ProfileFor(AccountType account)
        {
            return account == AccountType::Mova ? MovaProfile : DreameProfile;
        }

        // Like Python's urllib.parse.quote(text, safe='').
        std::string UrlEncode(std::string_view text)
        {
            static constexpr char HexDigits[] = "0123456789ABCDEF";
            std::string encoded;
            for (const char character : text)
            {
                const auto byte = static_cast<unsigned char>(character);
                if (std::isalnum(byte) || character == '-' || character == '.' || character == '_' || character == '~')
                {
                    encoded.push_back(character);
                }
                else
                {
                    encoded.push_back('%');
                    encoded.push_back(HexDigits[byte >> 4]);
                    encoded.push_back(HexDigits[byte & 0x0F]);
                }
            }
            return encoded;
        }

        std::string Truncate(const std::string& text)
        {
            return text.size() <= MaxBodyInErrors ? text : text.substr(0, MaxBodyInErrors) + "...";
        }

        // A field as text whether the cloud sent it as a string or a number.
        std::string TextOf(const json& object, const char* key, const std::string& fallback = {})
        {
            if (!object.is_object() || !object.contains(key))
            {
                return fallback;
            }
            const json& value = object[key];
            if (value.is_string())
            {
                return value.get<std::string>();
            }
            return value.is_number() ? value.dump() : fallback;
        }

        std::int64_t NumberOf(const json& object, const char* key, std::int64_t fallback)
        {
            if (object.is_object() && object.contains(key) && object[key].is_number_integer())
            {
                return object[key].get<std::int64_t>();
            }
            return fallback;
        }

        bool EqualsIgnoreCase(std::string_view left, std::string_view right)
        {
            return left.size() == right.size() &&
                   std::equal(left.begin(), left.end(), right.begin(), [](char leftChar, char rightChar)
                   {
                       return std::tolower(static_cast<unsigned char>(leftChar)) == std::tolower(static_cast<unsigned char>(rightChar));
                   });
        }

        // Command IDs start somewhere in 1 to 100 and count up, as in the integration.
        std::int64_t FirstRequestId()
        {
            std::random_device randomDevice;
            return std::uniform_int_distribution<std::int64_t>(1, 100)(randomDevice);
        }

        std::int64_t NowMilliseconds()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        std::string ListNames(const std::vector<DeviceInfo>& devices)
        {
            std::string names;
            for (const DeviceInfo& device : devices)
            {
                names += (names.empty() ? "" : ", ") + device.Name + " (" + device.Model + ")";
            }
            return names.empty() ? "none" : names;
        }
    }

    CloudClient::CloudClient(CloudOptions options)
        : options(std::move(options))
        , profile(ProfileFor(this->options.Account))
        , http(this->options.Country + std::string(profile.HostSuffix), ApiPort, this->options.Timeout)
        , visitorHash(Crypto::RandomHex(16))
        , requestId(FirstRequestId())
    {
    }

    CloudClient::~CloudClient() = default;

    void CloudClient::Login()
    {
        if (!refreshToken.empty() &&
            RequestToken("grant_type=refresh_token&scope=all&platform=ANDROID&type=account&refresh_token=" + refreshToken, true))
        {
            return;
        }
        refreshToken.clear();

        std::string body = "grant_type=password&scope=all&platform=ANDROID&type=account&username=" + UrlEncode(options.Username) +
                           "&password=" + Crypto::Md5Hex(options.Password + std::string(PasswordSalt));
        if (!countryCode.empty() && !language.empty())
        {
            body += "&country=" + countryCode + "&lang=" + language;
        }
        RequestToken(body, false);
    }

    bool CloudClient::RequestToken(const std::string& formBody, bool usingRefreshToken)
    {
        accessToken.clear();
        const Net::HttpResponse response = http.Post("/dreame-auth/oauth/token", AppHeaders(false), formBody,
                                                     "application/x-www-form-urlencoded");
        if (response.Status == 200)
        {
            const json data = json::parse(response.Body, nullptr, false);
            const std::string token = TextOf(data, "access_token");
            if (token.empty())
            {
                throw CloudError("sign-in returned no access token: " + Truncate(response.Body));
            }
            accessToken = token;
            refreshToken = TextOf(data, "refresh_token");
            accessTokenExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(NumberOf(data, "expires_in", 3600));
            userId = TextOf(data, "uid", userId);
            tenantId = TextOf(data, "tenant_id", tenantId);
            region = TextOf(data, "region", region);
            language = TextOf(data, "lang", language);
            countryCode = TextOf(data, "country", countryCode);
            Logging::Get("Dreame")->debug("signed in{}", usingRefreshToken ? " with the refresh token" : "");
            return true;
        }
        if (response.Status == 400 || response.Status == 401 || response.Status == 403)
        {
            if (usingRefreshToken)
            {
                return false;
            }
            throw AuthenticationError("sign-in rejected (HTTP " + std::to_string(response.Status) + "): " + Truncate(response.Body));
        }
        throw CloudError("sign-in failed (HTTP " + std::to_string(response.Status) + "): " + Truncate(response.Body));
    }

    void CloudClient::EnsureSession()
    {
        if (accessToken.empty() || std::chrono::steady_clock::now() + TokenRefreshMargin >= accessTokenExpiry)
        {
            Login();
        }
    }

    Net::HttpHeaders CloudClient::AppHeaders(bool includeSession) const
    {
        Net::HttpHeaders headers;
        headers.emplace_back("user-agent", profile.UserAgent);
        headers.emplace_back("dreame-meta", "cv=a_" + std::string(profile.AppVersion) +
                                                ";canvasHash=" + Crypto::Md5Hex(options.Username + "c").substr(0, 8) +
                                                ";webglHash=" + Crypto::Md5Hex(options.Username + "w").substr(0, 8) +
                                                ";visitorIdHash=" + visitorHash);
        headers.emplace_back("accept-encoding", "gzip");
        if (!region.empty() && !language.empty() && !countryCode.empty())
        {
            headers.emplace_back("dreame-rlc", Crypto::Aes128EcbBase64(profile.SigningKey, region + "|" + language + "|" + countryCode));
        }
        headers.emplace_back("tenant-id", tenantId.empty() ? std::string(profile.DefaultTenantId) : tenantId);
        headers.emplace_back("authorization", "Basic " + std::string(profile.BasicAuthorization));
        if (profile.SendsPasswordStyleHeader)
        {
            headers.emplace_back("dreame-psd", "new");
        }
        if (includeSession && !accessToken.empty())
        {
            headers.emplace_back("dreame-auth", accessToken);
        }
        return headers;
    }

    json CloudClient::ApiCall(const std::string& path, const json* parameters)
    {
        for (int attempt = 0;; ++attempt)
        {
            EnsureSession();
            const std::string body = parameters ? SignParameters(*parameters, NowMilliseconds(), profile.SigningKey).dump() : std::string();
            const Net::HttpResponse response = http.Post(path, AppHeaders(true), body, "application/json");
            Logging::Get("Dreame")->trace("POST {} -> {}", path, response.Status);

            if (response.Status == 401 && attempt == 0)
            {
                // Expired or revoked session: sign in again and retry once.
                accessToken.clear();
                continue;
            }
            if (response.Status == 401)
            {
                throw AuthenticationError("session rejected: " + Truncate(response.Body));
            }
            if (response.Status != 200)
            {
                throw CloudError(path + " failed (HTTP " + std::to_string(response.Status) + "): " + Truncate(response.Body));
            }

            json data = json::parse(response.Body, nullptr, false);
            if (data.is_discarded())
            {
                throw CloudError(path + " returned something other than JSON: " + Truncate(response.Body));
            }
            return data;
        }
    }

    std::vector<DeviceInfo> CloudClient::ListDevices()
    {
        const json response = ApiCall("/dreame-user-iot/iotuserbind/device/listV2", nullptr);
        if (NumberOf(response, "code", -1) != 0 || !response.contains("data") || !response["data"].contains("page") ||
            !response["data"]["page"].contains("records") || !response["data"]["page"]["records"].is_array())
        {
            throw CloudError("device list failed: " + Truncate(response.dump()));
        }

        std::vector<DeviceInfo> devices;
        for (const json& record : response["data"]["page"]["records"])
        {
            DeviceInfo device;
            device.Id = TextOf(record, "did");
            device.Model = TextOf(record, "model");
            device.BindDomain = TextOf(record, "bindDomain");
            device.MasterUid = TextOf(record, "masterUid");
            device.Name = TextOf(record, "customName");
            if (device.Name.empty() && record.contains("deviceInfo"))
            {
                device.Name = TextOf(record["deviceInfo"], "displayName", device.Model);
            }
            devices.push_back(std::move(device));
        }
        return devices;
    }

    const DeviceInfo& CloudClient::SelectDevice()
    {
        const std::vector<DeviceInfo> devices = ListDevices();
        auto found = devices.end();
        if (!options.Device.empty())
        {
            found = std::find_if(devices.begin(), devices.end(), [this](const DeviceInfo& device)
            {
                return EqualsIgnoreCase(device.Id, options.Device) || EqualsIgnoreCase(device.Name, options.Device);
            });
            if (found == devices.end())
            {
                throw CloudError("no device '" + options.Device + "' on this account; found: " + ListNames(devices));
            }
        }
        else
        {
            found = std::find_if(devices.begin(), devices.end(), [](const DeviceInfo& device)
            {
                return device.Model.find(".vacuum.") != std::string::npos;
            });
            if (found == devices.end())
            {
                throw CloudError("no robot vacuum on this account; found: " + ListNames(devices));
            }
        }
        selectedDevice = *found;
        return *selectedDevice;
    }

    std::map<PropertyId, std::int64_t> CloudClient::GetProperties(const std::vector<PropertyId>& properties)
    {
        if (!selectedDevice)
        {
            SelectDevice();
        }
        const DeviceInfo& device = *selectedDevice;
        const std::string hostLabel = device.BindDomain.substr(0, device.BindDomain.find('.'));
        const std::string path = "/dreame-iot-com" + (hostLabel.empty() ? std::string() : "-" + hostLabel) + "/device/sendCommand";

        json requested = json::array();
        for (const PropertyId& property : properties)
        {
            requested.push_back({{"did", std::to_string(property.ServiceId) + "." + std::to_string(property.PropertyNumber)},
                                 {"siid", property.ServiceId},
                                 {"piid", property.PropertyNumber}});
        }

        // The cloud sometimes answers success with no result; the integration retries once.
        json response;
        const json* result = nullptr;
        for (int attempt = 0; attempt < 2 && !result; ++attempt)
        {
            ++requestId;
            const json parameters = {{"did", device.Id},
                                     {"id", requestId},
                                     {"data", {{"did", device.Id}, {"id", requestId}, {"method", "get_properties"}, {"params", requested}}}};
            response = ApiCall(path, &parameters);
            if (response.contains("data") && response["data"].is_object() && response["data"].contains("result") &&
                response["data"]["result"].is_array())
            {
                result = &response["data"]["result"];
            }
            else if (!(response.contains("success") && response["success"].is_boolean() && response["success"].get<bool>()))
            {
                break;
            }
        }
        if (!result)
        {
            throw CloudError("the robot didn't answer (it may be offline): " + Truncate(response.dump()));
        }

        std::map<PropertyId, std::int64_t> values;
        for (const json& item : *result)
        {
            if (NumberOf(item, "code", -1) != 0 || !item.contains("value"))
            {
                continue;
            }
            const json& value = item["value"];
            if (!value.is_number_integer() && !value.is_boolean())
            {
                continue;
            }
            const PropertyId property{static_cast<std::int32_t>(NumberOf(item, "siid", 0)), static_cast<std::int32_t>(NumberOf(item, "piid", 0))};
            values[property] = value.is_boolean() ? (value.get<bool>() ? 1 : 0) : value.get<std::int64_t>();
        }
        return values;
    }

    PushChannel CloudClient::GetPushChannel()
    {
        EnsureSession();
        if (!selectedDevice)
        {
            SelectDevice();
        }
        const DeviceInfo& device = *selectedDevice;
        const std::size_t portSeparator = device.BindDomain.rfind(':');
        if (portSeparator == std::string::npos)
        {
            throw CloudError("device has no MQTT host (bindDomain '" + device.BindDomain + "')");
        }

        PushChannel channel;
        channel.Host = device.BindDomain.substr(0, portSeparator);
        channel.Port = std::stoi(device.BindDomain.substr(portSeparator + 1));
        // Same client ID shape as the integration: "p_" + md5(did + "mqtt" + visitor hash).
        channel.ClientId = "p_" + Crypto::Md5Hex(device.Id + "mqtt" + visitorHash);
        channel.Username = userId;
        channel.Password = accessToken;
        channel.Topic = "/status/" + device.Id + "/" + (device.MasterUid.empty() ? userId : device.MasterUid) + "/" + device.Model +
                        "/" + options.Country + "/";
        return channel;
    }
}
