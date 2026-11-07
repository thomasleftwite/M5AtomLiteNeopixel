// Compile with C++14 or later. Static assertions exercise the production code.
#include "../firmware/AtomNeoPixel/IpAddressBlink.h"

constexpr bool checkSequence(uint32_t address, const char *sequence, uint32_t start) {
  IpAddressBlink blink;
  blink.begin(address, start);
  uint32_t elapsed = 0;
  for (unsigned i = 0; sequence[i]; ++i) {
    if (sequence[i] == ' ' || sequence[i] == '|') continue;
    const uint32_t on = sequence[i] == '-' ? 600 : 200;
    if (!blink.isOn(start + elapsed) || !blink.isOn(start + elapsed + on - 1)) return false;
    elapsed += on;
    const uint32_t off = !sequence[i + 1] ? 3000 : sequence[i + 1] == '|' ? 1400
                                               : sequence[i + 1] == ' ' ? 800 : 200;
    if (blink.isOn(start + elapsed) || blink.isOn(start + elapsed + off - 1)) return false;
    elapsed += off;
  }
  // Exact cycle boundary, repeated cycles and a delayed render frame.
  return blink.isOn(start + elapsed) && blink.isOn(start + 2 * elapsed) &&
         blink.isOn(start + 5 * elapsed + 199) &&
         !blink.isOn(start + 5 * elapsed + (sequence[0] == '-' ? 600 : 200));
}

constexpr bool checkOctets(unsigned first, unsigned end) {
  constexpr const char *patterns[] = {"--", ".", "..", "...", "....", ".....",
                                     "-.", "-..", "-...", "-...."};
  for (unsigned octet = first; octet < end; ++octet) {
    char sequence[72]{};
    unsigned position = 0;
    for (unsigned byte = 0; byte < 4; ++byte) {
      for (unsigned divisor = 100; divisor; divisor /= 10) {
        const char *pattern = patterns[(octet / divisor) % 10];
        for (unsigned pulse = 0; pattern[pulse]; ++pulse) sequence[position++] = pattern[pulse];
        if (byte != 3 || divisor != 1) sequence[position++] = divisor == 1 ? '|' : ' ';
      }
    }
    if (!checkSequence(octet * 0x01010101u, sequence, 0xfffffff0u)) return false;
  }
  return true;
}

constexpr bool checkRestart() {
  IpAddressBlink blink;
  if (blink.isOn(100)) return false;
  blink.begin(0xc0a80001u, 100);
  if (blink.isOn(300)) return false;
  blink.begin(0x00000000u, 600);
  return blink.isOn(600) && blink.isOn(1199) && !blink.isOn(1200) && blink.isOn(1400);
}

static_assert(checkSequence(0xc0a80001u, ". -.... ..|. -. -...|-- -- --|-- -- .", 0),
              "192.168.000.001 must match the requested pulses and gaps");
static_assert(checkRestart(), "A new address must restart at its first pulse");
static_assert(checkOctets(0, 64), "Octets 000-063, timing boundaries and clock rollover");
static_assert(checkOctets(64, 128), "Octets 064-127, timing boundaries and clock rollover");
static_assert(checkOctets(128, 192), "Octets 128-191, timing boundaries and clock rollover");
static_assert(checkOctets(192, 256), "Octets 192-255, timing boundaries and clock rollover");
