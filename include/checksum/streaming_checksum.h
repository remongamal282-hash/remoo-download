#ifndef REMO_DOWNLOAD_CHECKSUM_STREAMING_CHECKSUM_H
#define REMO_DOWNLOAD_CHECKSUM_STREAMING_CHECKSUM_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace remo {
namespace checksum {

class StreamingChecksum {
public:
    StreamingChecksum();
    ~StreamingChecksum();

    void begin();
    void update(const void* data, size_t length);
    std::string md5() const;
    std::string sha256() const;
    std::string md5Hex() const;
    std::string sha256Hex() const;

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace checksum
} // namespace remo

#endif // REMO_DOWNLOAD_CHECKSUM_STREAMING_CHECKSUM_H
