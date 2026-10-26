#include "Dreame/Signing.h"

#include "Crypto/Crypto.h"

#include <vector>

namespace ClankerCleaner::Dreame
{
    std::string SpliceForSigning(const nlohmann::json& parameters, bool topLevel)
    {
        // nlohmann::json objects keep their keys sorted, as the Python version sorts them.
        std::vector<std::string> parts;
        for (const auto& [key, value] : parameters.items())
        {
            if (value.is_object())
            {
                const std::string inner = SpliceForSigning(value, false);
                parts.push_back(inner.empty() ? key + "=]" : key + "=[" + inner + "]");
            }
            else if (value.is_array())
            {
                if (topLevel)
                {
                    parts.push_back(key + "=" + value.dump());
                }
            }
            else if (value.is_boolean())
            {
                parts.push_back(key + "=" + (value.get<bool>() ? "true" : "false"));
            }
            else if (value.is_null())
            {
                parts.push_back(key + "=null");
            }
            else if (topLevel)
            {
                parts.push_back(key + "=" + (value.is_string() ? value.get<std::string>() : value.dump()));
            }
            else
            {
                parts.push_back(key + "=" + value.dump());
            }
        }

        std::string spliced;
        for (const std::string& part : parts)
        {
            if (!spliced.empty())
            {
                spliced += '&';
            }
            spliced += part;
        }
        return spliced;
    }

    nlohmann::json SignParameters(nlohmann::json parameters, std::int64_t timestampMilliseconds, std::string_view signingKey)
    {
        const std::string signedText = SpliceForSigning(parameters) + std::to_string(timestampMilliseconds) + std::string(signingKey);
        parameters["sign"] = Crypto::Md5Hex(signedText);
        parameters["timestamp"] = timestampMilliseconds;
        return parameters;
    }
}
