#include "checksum/streaming_checksum.h"

#include <openssl/md5.h>
#include <openssl/sha.h>

#include <iomanip>
#include <sstream>

namespace remo {
namespace checksum {

class StreamingChecksum::Impl {
public:
    MD5_CTX md5Context;
    SHA256_CTX sha256Context;
    bool initialized = false;

    void ensureInitialized() {
        if (!initialized) {
            MD5_Init(&md5Context);
            SHA256_Init(&sha256Context);
            initialized = true;
        }
    }
};

StreamingChecksum::StreamingChecksum()
    : d(std::make_unique<Impl>())
{
}

StreamingChecksum::~StreamingChecksum() = default;

void StreamingChecksum::begin() {
    MD5_Init(&d->md5Context);
    SHA256_Init(&d->sha256Context);
    d->initialized = true;
}

void StreamingChecksum::update(const void* data, size_t length) {
    d->ensureInitialized();
    MD5_Update(&d->md5Context, data, length);
    SHA256_Update(&d->sha256Context, data, length);
}

std::string StreamingChecksum::md5() const {
    d->ensureInitialized();
    MD5_CTX context = d->md5Context;
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &context);
    return std::string(reinterpret_cast<char*>(hash), MD5_DIGEST_LENGTH);
}

std::string StreamingChecksum::sha256() const {
    d->ensureInitialized();
    SHA256_CTX context = d->sha256Context;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &context);
    return std::string(reinterpret_cast<char*>(hash), SHA256_DIGEST_LENGTH);
}

std::string StreamingChecksum::md5Hex() const {
    d->ensureInitialized();
    MD5_CTX context = d->md5Context;
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &context);
    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::string StreamingChecksum::sha256Hex() const {
    d->ensureInitialized();
    SHA256_CTX context = d->sha256Context;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &context);
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}

} // namespace checksum
} // namespace remo
