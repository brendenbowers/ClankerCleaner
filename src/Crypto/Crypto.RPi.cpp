#include "Crypto/Crypto.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <memory>
#include <stdexcept>
#include <vector>

namespace ClankerCleaner::Crypto
{
    namespace
    {
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
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digestLength = 0;
        if (EVP_Digest(data.data(), data.size(), digest, &digestLength, EVP_md5(), nullptr) != 1)
        {
            throw std::runtime_error("MD5 failed");
        }
        return ToHex(digest, digestLength);
    }

    std::string Aes128EcbBase64(std::string_view key, std::string_view plainText)
    {
        if (key.size() != 16)
        {
            throw std::invalid_argument("AES-128 key must be 16 bytes");
        }

        std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
        // Padding is on by default (PKCS#7), so the output is at most one block longer.
        std::vector<unsigned char> cipherText(plainText.size() + 16);
        int writtenLength = 0;
        int finalLength = 0;
        const auto* keyBytes = reinterpret_cast<const unsigned char*>(key.data());
        const auto* plainBytes = reinterpret_cast<const unsigned char*>(plainText.data());
        if (!context ||
            EVP_EncryptInit_ex(context.get(), EVP_aes_128_ecb(), nullptr, keyBytes, nullptr) != 1 ||
            EVP_EncryptUpdate(context.get(), cipherText.data(), &writtenLength, plainBytes, static_cast<int>(plainText.size())) != 1 ||
            EVP_EncryptFinal_ex(context.get(), cipherText.data() + writtenLength, &finalLength) != 1)
        {
            throw std::runtime_error("AES encryption failed");
        }

        const int cipherLength = writtenLength + finalLength;
        std::string encoded(4 * ((cipherLength + 2) / 3), '\0');
        const int encodedLength = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()), cipherText.data(), cipherLength);
        encoded.resize(static_cast<std::size_t>(encodedLength));
        return encoded;
    }

    std::string RandomHex(std::size_t byteCount)
    {
        std::vector<unsigned char> bytes(byteCount);
        if (RAND_bytes(bytes.data(), static_cast<int>(byteCount)) != 1)
        {
            throw std::runtime_error("random number generator failed");
        }
        return ToHex(bytes.data(), byteCount);
    }
}
