// Host unit tests for the Wiegand encoding of P250: g++ -std=c++17 -I../src/src/PluginStructs test_wiegand.cpp
#include "P250_wiegand.h"

#include <cstdio>
#include <initializer_list>

static int failures = 0;

#define CHECK(cond)                                                \
  do {                                                             \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } \
  } while (0)

static int bitAt(uint64_t frame, int frameBits, int pos) { // pos 1..frameBits, MSB first
  return (frame >> (frameBits - pos)) & 1;
}

int main() {
  using namespace P250_wiegand;

  // 26 bit: bits 1..13 even parity, bits 14..26 odd parity, data = bits 2..25
  for (uint32_t fc = 0; fc < 256; fc += 5) {
    for (uint32_t card = 0; card < 65536; card += 97) {
      const uint32_t data  = (fc << 16) | card;
      const uint64_t frame = addParity(data, 26);
      int first = 0, second = 0;

      for (int i = 1; i <= 13; ++i) { first += bitAt(frame, 26, i); }
      for (int i = 14; i <= 26; ++i) { second += bitAt(frame, 26, i); }
      CHECK(first % 2 == 0);
      CHECK(second % 2 == 1);
      CHECK(((frame >> 1) & 0xFFFFFF) == data);
      CHECK((frame >> 26) == 0);
    }
  }

  // Known 26 bit frames
  CHECK(addParity(0, 26) == 0x1);                       // P0=0, data 0, P1=1
  CHECK(addParity((123u << 16) | 4567u, 26) == 0x2F623AE);

  // 34 bit: bits 1..17 even, bits 18..34 odd
  for (uint64_t data : { 0ull, 1ull, 0xDEADBEEFull, 0xFFFFFFFFull, 0x12345678ull }) {
    const uint64_t frame = addParity(data, 34);
    int first = 0, second = 0;

    for (int i = 1; i <= 17; ++i) { first += bitAt(frame, 34, i); }
    for (int i = 18; i <= 34; ++i) { second += bitAt(frame, 34, i); }
    CHECK(first % 2 == 0);
    CHECK(second % 2 == 1);
    CHECK(((frame >> 1) & 0xFFFFFFFFull) == data);
  }

  // Data wider than the frame is truncated
  CHECK(((addParity(0x1FFFFFFull, 26) >> 1) & 0xFFFFFF) == 0xFFFFFF);

  // Keypad
  CHECK(keyValue('0') == 0);
  CHECK(keyValue('9') == 9);
  CHECK(keyValue('*') == 10);
  CHECK(keyValue('#') == 11);
  CHECK(keyValue('a') == 10);
  CHECK(keyValue('F') == 15);
  CHECK(keyValue('x') == -1);
  CHECK(keyFrame8(1) == 0xE1);
  CHECK(keyFrame8(11) == 0x4B);
  CHECK(keyFrame8(0) == 0xF0);

  if (failures == 0) { std::printf("All tests passed\n"); }
  return failures == 0 ? 0 : 1;
}
