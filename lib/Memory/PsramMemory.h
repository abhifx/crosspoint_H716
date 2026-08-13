#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <memory>

/**
 * PSRAM-backed unique_ptr helper for large buffers.
 * Ensures the allocation lands in SPIRAM even if it's not integrated into the main heap.
 */
template <typename T>
std::unique_ptr<T[]> makeUniquePsram(size_t count) {
  void* ptr = heap_caps_malloc(count * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ptr) return nullptr;
  memset(ptr, 0, count * sizeof(T));
  return std::unique_ptr<T[]>(static_cast<T*>(ptr));
}
