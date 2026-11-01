#include "Crypto/Crypto.h"

#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <mbedtls/md5.h>

#include <esp_random.h>

#include <stdexcept>
#include <vector>

namespace ClankerCleaner::Crypto
{
    namespace
    {
        constexpr std::size_t AesBlockSize = 16;

        struct AesContext
        {
            AesContext() { mbedtls_aes_init(&Value); }
            ~AesContext() { mbedtls_aes_free(&Value); }
            AesContext(const AesContext&) = delete;
            AesContext& operator=(const AesContext&) = delete;

            mbedtls_aes_context Value;
        };

        std::string ToHex(const unsigned char* bytes, std::size_t byteCount)
        {
            static constexpr char HexDigits[] = "0123456789abcdef";
            std::string hex;
            hex.reserve(byteCount * 2);
            for (std::size_t byteIndex = 0; byteIndex < byteCount; ++byteIndex)
            {
                hex.push_back(HexDigits[bytes[byteIndex] >> 4]);
                hex.push_back(HexDigits[bytes[byteIndex] & 0x0F]);
            }
            return hex;
        }
    }

    std::string Md5Hex(std::string_view data)
    {
        unsigned char digest[16];
        if (mbedtls_md5(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest) != 0)
        {
            throw std::runtime_error("MD5 failed");
        }
        return ToHex(digest, sizeof(digest));
    }

    std::string Aes128EcbBase64(std::string_view key, std::string_view plainText)
    {
        if (key.size() != 16)
        {
            throw std::invalid_argument("AES-128 key must be 16 bytes");
        }

        // PKCS#7: pad to a whole number of blocks, always adding at least one byte.
        const std::size_t padding = AesBlockSize - plainText.size() % AesBlockSize;
        std::vector<unsigned char> blocks(plainText.begin(), plainText.end());
        blocks.insert(blocks.end(), padding, static_cast<unsigned char>(padding));

        AesContext context;
        if (mbedtls_aes_setkey_enc(&context.Value, reinterpret_cast<const unsigned char*>(key.data()), 128) != 0)
        {
            throw std::runtime_error("AES encryption failed");
        }
        for (std::size_t offset = 0; offset < blocks.size(); offset += AesBlockSize)
        {
            if (mbedtls_aes_crypt_ecb(&context.Value, MBEDTLS_AES_ENCRYPT, &blocks[offset], &blocks[offset]) != 0)
            {
                throw std::runtime_error("AES encryption failed");
            }
        }

        // Room for the terminating NUL mbedtls writes.
        std::string encoded(4 * ((blocks.size() + 2) / 3) + 1, '\0');
        std::size_t encodedLength = 0;
        if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(encoded.data()), encoded.size(), &encodedLength,
                                  blocks.data(), blocks.size()) != 0)
        {
            throw std::runtime_error("base64 encoding failed");
        }
        encoded.resize(encodedLength);
        return encoded;
    }

    std::string RandomHex(std::size_t byteCount)
    {
        // Hardware random numbers; truly random once Wi-Fi or Bluetooth is running.
        std::vector<unsigned char> bytes(byteCount);
        esp_fill_random(bytes.data(), byteCount);
        return ToHex(bytes.data(), byteCount);
    }
}
