#pragma once

#include <stdint.h>

// A complete IPv4 address, four zero-padded decimal octets, in green pulses.
// Durations include the entire dark gap, rather than adding gaps together.
class IpAddressBlink {
 public:
  constexpr void begin(uint32_t address, uint32_t now) {
    count_ = 0;
    cycleMs_ = 0;
    phaseMs_ = 0;
    lastTick_ = now;
    for (int shift = 24; shift >= 0; shift -= 8) {
      const uint8_t octet = (address >> shift) & 255;
      for (unsigned divisor = 100; divisor != 0; divisor /= 10) {
        const uint8_t digit = (octet / divisor) % 10;
        const uint8_t pulses = digit == 0 ? 2 : digit <= 5 ? digit : digit - 4;
        for (uint8_t pulse = 0; pulse < pulses; ++pulse) {
          append(digit == 0 || (digit >= 6 && pulse == 0) ? 600 : 200);
          const bool lastPulse = pulse + 1 == pulses;
          const bool lastOctetDigit = divisor == 1;
          const bool lastDigit = shift == 0 && lastOctetDigit;
          append(lastPulse ? (lastDigit ? 3000 : lastOctetDigit ? 1400 : 800) : 200);
        }
      }
    }
  }

  constexpr bool isOn(uint32_t now) {
    if (cycleMs_ == 0) return false;
    // Incremental unsigned subtraction also works when millis() wraps.
    phaseMs_ = (phaseMs_ + (now - lastTick_) % cycleMs_) % cycleMs_;
    lastTick_ = now;
    uint32_t remaining = phaseMs_;
    for (uint8_t i = 0; i < count_; ++i) {
      if (remaining < durations_[i]) return i % 2 == 0;
      remaining -= durations_[i];
    }
    return false;
  }

 private:
  constexpr void append(uint16_t duration) {
    durations_[count_++] = duration;
    cycleMs_ += duration;
  }

  uint16_t durations_[12 * 5 * 2]{};
  uint8_t count_ = 0;
  uint32_t cycleMs_ = 0;
  uint32_t phaseMs_ = 0;
  uint32_t lastTick_ = 0;
};
