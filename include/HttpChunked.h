#pragma once
#include <stddef.h>
#include <stdint.h>

template <typename Read, typename Write>
bool decodeHttpChunks(Read read, Write write, char *buffer, size_t capacity) {
  if (!capacity) return false;
  auto byte = [&]() -> int {
    uint8_t c;
    return read(&c, 1) == 1 ? c : -1;
  };
  auto line = [&](auto consume) -> bool {
    for (size_t i = 0; i < 4096; i++) {
      const int c = byte();
      if (c < 0 || c == '\n') return false;
      if (c == '\r') return byte() == '\n';
      if (!consume(c)) return false;
    }
    return false;
  };
  for (;;) {
    size_t size = 0;
    bool digits = false, extension = false;
    if (!line([&](int c) {
      if (extension) return true;
      if (c == ';' && digits) { extension = true; return true; }
      const int hex = c >= '0' && c <= '9' ? c - '0' :
                      c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                      c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (hex < 0 || size > (SIZE_MAX - size_t(hex)) / 16) return false;
      size = size * 16 + size_t(hex);
      digits = true;
      return true;
    }) || !digits) return false;
    if (!size) {
      for (;;) {
        bool empty = true;
        if (!line([&](int) { empty = false; return true; })) return false;
        if (empty) return true;
      }
    }
    while (size) {
      const size_t want = size < capacity ? size : capacity;
      const int n = read(reinterpret_cast<uint8_t *>(buffer), want);
      if (n <= 0 || size_t(n) > want || !write(buffer, size_t(n))) return false;
      size -= size_t(n);
    }
    if (byte() != '\r' || byte() != '\n') return false;
  }
}
