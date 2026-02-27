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

namespace sonic_json {
namespace internal {
namespace neon {

using sonic_json::internal::common::EqBytes4;
using sonic_json::internal::common::SkipLiteral;

#include "../common/arm_common/skip.inc.h"

sonic_force_inline bool SkipContainer(const uint8_t *data, size_t &pos,
                                      size_t len, uint8_t left, uint8_t right) {
  return skip_container<simd8x64<uint8_t>>(data, pos, len, left, right);
}

// TODO: optimize by removing bound checking.
sonic_force_inline uint8_t skip_space(const uint8_t *data, size_t &pos,
                                      size_t &nonspace_bits_end,
                                      uint64_t &nonspace_bits) {
  // fast path for single space
  if (!IsSpace(data[pos++])) return data[pos - 1];
  if (!IsSpace(data[pos++])) return data[pos - 1];

  uint64_t nonspace;

  // 1. 如果当前的 pos 已经超出了我们缓存的 16 字节块
  if (pos >= nonspace_bits_end) {
  found_space:
    while (1) {
      nonspace = GetNonSpaceBits(data + pos);
      if (nonspace) {
        // NEON 每次处理 16 字节
        nonspace_bits_end = pos + 16;       
        // 每 4 bits 代表 1 个 byte，所以要 >> 2
        pos += TrailingZeroes(nonspace) >> 2; 
        nonspace_bits = nonspace;
        return data[pos++];
      } else {
        pos += 16;
      }
    }
    sonic_assert(false && "!should not happen");
  }

  // 2. 如果当前的 pos 还在缓存块内部 (Reuse 核心逻辑)
  sonic_assert(pos + 16 > nonspace_bits_end);
  size_t block_start = nonspace_bits_end - 16;
  sonic_assert(pos >= block_start);
  
  // 计算当前 pos 在 block 内的字节偏移
  size_t byte_pos = pos - block_start;
  
  // 转换为 bit 偏移 (因为 1 byte = 4 bits)
  size_t bit_pos = byte_pos * 4;
  
  // 生成掩码，将当前 pos 之前的 bits 全部置 0
  // 注意：因为 byte_pos 最大是 15，所以 bit_pos 最大是 60，这里 1ull << bit_pos 不会溢出 64 位
  uint64_t mask = (1ull << bit_pos) - 1;
  nonspace = nonspace_bits & (~mask);

  if (nonspace == 0) {
    // 缓存块剩下的部分全是空格，直接跳到块尾，继续搜索
    pos = nonspace_bits_end;
    goto found_space;
  }

  // 缓存块中还有非空字符，直接算出相对 block_start 的绝对偏移
  pos = block_start + (TrailingZeroes(nonspace) >> 2);
  return data[pos++];
}

}  // namespace neon
}  // namespace internal
}  // namespace sonic_json

#undef VEC_LEN
