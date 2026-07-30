#include <gtest/gtest.h>

#include <string>

#include "checksum/streaming_checksum.h"

using namespace remo::checksum;

TEST(StreamingChecksumTest, ComputesKnownHashes) {
    StreamingChecksum checksum;
    const std::string data = "abc";

    checksum.update(data.data(), data.size());

    EXPECT_EQ(checksum.md5Hex(), "900150983cd24fb0d6963f7d28e17f72");
    EXPECT_EQ(checksum.sha256Hex(), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(StreamingChecksumTest, ReadingDigestDoesNotFinalizeStreamState) {
    StreamingChecksum checksum;
    const std::string first = "ab";
    const std::string second = "c";

    checksum.update(first.data(), first.size());
    const auto partial = checksum.md5Hex();
    checksum.update(second.data(), second.size());

    EXPECT_EQ(partial, "187ef4436122d1cc2f40dc2b92f0eba0");
    EXPECT_EQ(checksum.md5Hex(), "900150983cd24fb0d6963f7d28e17f72");
    EXPECT_EQ(checksum.md5Hex(), "900150983cd24fb0d6963f7d28e17f72");
}
