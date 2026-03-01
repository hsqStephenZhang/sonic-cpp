/*
 * Copyright 2022 ByteDance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#define VEC_LEN 16

#include <sonic/internal/arch/common/skip_common.h>
#include <sonic/internal/utils.h>

#include "base.h"
#include "simd.h"
#include "unicode.h"

namespace sonic_json {
namespace internal {
namespace neon {

using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;

#include "../common/arm_common/skip.inc.h"

sonic_force_inline bool SkipContainer(const uint8_t* data, size_t& pos,
                                      size_t len, uint8_t left, uint8_t right) {
  return skip_container<simd8x64<uint8_t>>(data, pos, len, left, right);
}

// TODO: optimize by removing bound checking.
sonic_force_inline uint8_t skip_space(const uint8_t* data, size_t& pos,
                                      size_t& nonspace_bits_end,
                                      uint64_t& nonspace_bits) {
  // fast path for single space
  if (!IsSpace(data[pos++])) return data[pos - 1];
  if (!IsSpace(data[pos++])) return data[pos - 1];

  uint64_t nonspace;
  // current pos is out of block
  if (pos >= nonspace_bits_end) {
  found_space:
    while (1) {
      nonspace = GetNonSpaceBits64Bit(data + pos);
      if (nonspace) {
        nonspace_bits_end = pos + 64;
        pos += TrailingZeroes(nonspace);
        nonspace_bits = nonspace;
        return data[pos++];
      } else {
        pos += 64;
      }
    }
    sonic_assert(false && "!should not happen");
  }

  // current pos is in block
  sonic_assert(pos + 64 > nonspace_bits_end);
  size_t block_start = nonspace_bits_end - 64;
  sonic_assert(pos >= block_start);
  size_t bit_pos = pos - block_start;
  uint64_t mask = (1ull << bit_pos) - 1;
  nonspace = nonspace_bits & (~mask);
  if (nonspace == 0) {
    pos = nonspace_bits_end;
    goto found_space;
  }
  pos = block_start + TrailingZeroes(nonspace);
  return data[pos++];
}

}  // namespace neon
}  // namespace internal
}  // namespace sonic_json

#undef VEC_LEN
