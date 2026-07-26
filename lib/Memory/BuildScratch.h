#pragma once

#include <cstddef>
#include <cstdint>

namespace buildscratch {
void lend(uint8_t* buf, size_t len);
void reclaim();
uint8_t* claim(size_t minLen, size_t* lenOut = nullptr);
void release(const uint8_t* p);
}  // namespace buildscratch
