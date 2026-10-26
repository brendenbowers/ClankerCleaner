#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// Hashing and encryption helpers; Crypto.RPi.cpp implements them with OpenSSL.
namespace ClankerCleaner::Crypto
{
    // Lower-case hex MD5 of the bytes.
    std::string Md5Hex(std::string_view data);

    // AES-128-ECB with PKCS#7 padding, base64 encoded. key must be 16 bytes.
    std::string Aes128EcbBase64(std::string_view key, std::string_view plainText);

    // byteCount random bytes from the system's secure generator, as lower-case hex.
    std::string RandomHex(std::size_t byteCount);
}
