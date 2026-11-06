#pragma once

#include <stdint.h>
#include <stddef.h>

constexpr uint8_t LED_COUNT_MAX = 15;
constexpr uint8_t LED_BRIGHTNESS_MAX = 32;  // Hardware PWM on a scale of 0-255.

enum Pattern : uint8_t { Off, Solid, Blink, Fade, Rainbow };

struct LedState {
  uint8_t pattern;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  uint8_t speed;  // Cycles per minute for Rainbow, 0–100 for the other patterns.
  uint8_t ledCount;
};

constexpr size_t LEGACY_STATE_SIZE = 6;
constexpr size_t STATE_SIZE = 7;
constexpr LedState DEFAULT_STATE{Off, 255, 255, 255, 50, 50, LED_COUNT_MAX};

inline void encodeState(const LedState &state, uint8_t (&bytes)[STATE_SIZE]) {
  bytes[0] = state.pattern;
  bytes[1] = state.red;
  bytes[2] = state.green;
  bytes[3] = state.blue;
  bytes[4] = state.brightness;
  bytes[5] = state.speed;
  bytes[6] = state.ledCount;
}

inline bool decodeState(const uint8_t *bytes, size_t size, LedState &state) {
  const bool legacy = size == LEGACY_STATE_SIZE;
  if ((!legacy && size != STATE_SIZE) || bytes[0] > (legacy ? Fade : Rainbow) || bytes[4] > 100) {
    return false;
  }
  const uint8_t speed = bytes[5];
  const uint8_t ledCount = legacy ? state.ledCount : bytes[6];
  if (ledCount < 1 || ledCount > LED_COUNT_MAX ||
      (bytes[0] == Rainbow ? speed < 1 || speed > 60 : speed > 100)) {
    return false;
  }
  state = {bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], speed, ledCount};
  return true;
}

constexpr uint16_t rainbowHue(uint32_t elapsedMs, uint8_t cyclesPerMinute) {
  constexpr uint32_t MS_PER_MINUTE = 60000;
  const uint32_t phase = ((elapsedMs % MS_PER_MINUTE) * cyclesPerMinute) % MS_PER_MINUTE;
  return phase * 65536u / MS_PER_MINUTE;
}

inline uint8_t frameBrightness(const LedState &state, uint32_t elapsedMs) {
  const uint16_t level = state.brightness * LED_BRIGHTNESS_MAX / 100;
  const uint16_t period = 2400 - state.speed * 22;  // 2.4 s to 0.2 s.
  const uint16_t phase = elapsedMs % period;
  switch (state.pattern) {
    case Solid:
    case Rainbow: return level;
    case Blink: return phase < period / 2 ? level : 0;
    case Fade: {
      const uint16_t ramp = phase < period / 2 ? phase : period - phase;
      return level * ramp / (period / 2);
    }
    default: return 0;
  }
}
