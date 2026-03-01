#include <string>
#include <vector>
#include <cstring>

#include "gtest/gtest.h"
#include "sonic/dom/parser.h"

// Check for NEON support
#if defined(__ARM_NEON) || defined(__aarch64__)
#include "sonic/internal/arch/neon/unicode.h"
#include "sonic/internal/arch/neon/skip.h"
#endif

namespace {

using namespace sonic_json;

TEST(GetNonSpace, Basic) {
#if defined(__ARM_NEON) || defined(__aarch64__)
  // Align data to 64 bytes to ensure SIMD loads are safe (though usually unaligned loads are fine on NEON)
  alignas(64) uint8_t data[128];
  // Initialize with spaces
  std::memset(data, ' ', 128);

  // Set specific non-space characters
  data[0] = 'a';
  data[63] = 'b';
  data[10] = '\n'; // newline is considered space
  data[20] = '\t'; // tab is considered space
  
  // Test GetNonSpaceBits64Bit
  uint64_t bits = sonic_json::internal::neon::GetNonSpaceBits64Bit(data);
  
  // Verify bitmask
  // Bit 0 should be set for 'a'
  EXPECT_EQ(bits & 1, 1);
  // Bit 63 should be set for 'b'
  EXPECT_EQ((bits >> 63) & 1, 1);
  // Bit 10 (newline) should be 0
  EXPECT_EQ((bits >> 10) & 1, 0);
  // Bit 20 (tab) should be 0
  EXPECT_EQ((bits >> 20) & 1, 0);
  
  // Verify exact value
  uint64_t expected = (1ULL << 0) | (1ULL << 63);
  EXPECT_EQ(bits, expected);

  // Test skip_space
  size_t pos = 0;
  size_t nonspace_bits_end = 0;
  uint64_t nonspace_bits = 0;

  // First call should find 'a'
  uint8_t c = sonic_json::internal::neon::skip_space(data, pos, nonspace_bits_end, nonspace_bits);
  EXPECT_EQ(c, 'a');
  EXPECT_EQ(pos, 1);
  // nonspace_bits_end should be updated to 64 (start 0 + 64)
  EXPECT_EQ(nonspace_bits_end, 64);

  // Second call should find 'b'
  c = sonic_json::internal::neon::skip_space(data, pos, nonspace_bits_end, nonspace_bits);
  EXPECT_EQ(c, 'b');
  EXPECT_EQ(pos, 64);
  
  // Verify finding something in next block
  data[65] = 'c';
  // Reset for safety or continue?
  // If we continue:
  // pos=64. nonspace_bits_end=64.
  // pos >= nonspace_bits_end, so it should fetch new bits.
  // data + 64 starts next block.
  // 'c' is at index 1 in next block (64+1).
  
  c = sonic_json::internal::neon::skip_space(data, pos, nonspace_bits_end, nonspace_bits);
  EXPECT_EQ(c, 'c');
  EXPECT_EQ(pos, 66);
  EXPECT_EQ(nonspace_bits_end, 128);

#else
  // Not on ARM, skip test
  GTEST_SKIP() << "Skipping NEON tests on non-ARM architecture";
#endif
}

}