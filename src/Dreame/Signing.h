#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

// Request signing for the Dreame/MOVA cloud API, matching _spliced/_signed in the
// Tasshack/dreame-vacuum integration's protocol.py.
namespace ClankerCleaner::Dreame
{
    // The text that gets signed: keys sorted and joined with '&'; nested objects in [...] with
    // their values JSON-encoded; arrays included only at the top level.
    std::string SpliceForSigning(const nlohmann::json& parameters, bool topLevel = true);

    // Returns parameters plus "sign" (MD5 of the spliced text, timestamp and key) and "timestamp".
    nlohmann::json SignParameters(nlohmann::json parameters, std::int64_t timestampMilliseconds, std::string_view signingKey);
}
