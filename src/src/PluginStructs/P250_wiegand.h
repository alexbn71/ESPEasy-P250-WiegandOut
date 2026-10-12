#ifndef PLUGINSTRUCTS_P250_WIEGAND_H
#define PLUGINSTRUCTS_P250_WIEGAND_H

// Wiegand frame encoding used by P250, without ESPEasy dependencies (unit-tested on the host, see test/).

#include <stdint.h>

namespace P250_wiegand {

// Standard Wiegand frame of frameBits bits (4..64): 1 even parity bit over the first half of the data bits,
// the frameBits - 2 data bits (MSB first), 1 odd parity bit over the second half.
inline uint64_t addParity(uint64_t data, uint8_t frameBits) {
  const uint8_t  dataBits = frameBits - 2;
  const uint8_t  lowBits  = dataBits / 2;
  const uint64_t dataMask = dataBits >= 64 ? ~0ull : ((1ull << dataBits) - 1);

  data &= dataMask;
  const uint64_t lowHalf  = data & ((1ull << lowBits) - 1);
  const uint64_t highHalf = data >> lowBits;

  const uint8_t evenBit = __builtin_popcountll(highHalf) & 1;      // makes the 1-count of the first half even
  const uint8_t oddBit  = (__builtin_popcountll(lowHalf) & 1) ^ 1; // makes the 1-count of the second half odd

  return (static_cast<uint64_t>(evenBit) << (frameBits - 1)) | (data << 1) | oddBit;
}

// Keypad key value: 0-9, * = 10, # = 11, A-F = 10-15, -1 = invalid
inline int8_t keyValue(char c) {
  if ((c >= '0') && (c <= '9')) { return c - '0'; }
  if (c == '*') { return 10; }
  if (c == '#') { return 11; }
  if ((c >= 'A') && (c <= 'F')) { return c - 'A' + 10; }
  if ((c >= 'a') && (c <= 'f')) { return c - 'a' + 10; }
  return -1;
}

// 8 bit keypad frame: high nibble = inverted key, low nibble = key
inline uint8_t keyFrame8(uint8_t key) {
  return static_cast<uint8_t>(((~key & 0x0F) << 4) | (key & 0x0F));
}

} // namespace P250_wiegand

#endif // ifndef PLUGINSTRUCTS_P250_WIEGAND_H
