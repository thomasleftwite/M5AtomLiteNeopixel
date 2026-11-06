#pragma once

#include <stdint.h>
#include <stddef.h>

// Set this to the number of external pixels actually connected.
constexpr int LED_COUNT = 15;
static_assert(LED_COUNT >= 1 && LED_COUNT <= 15, "LED_COUNT must be 1-15");
constexpr uint8_t LED_BRIGHTNESS_MAX = 32;  // Hardware PWM on a scale of 0-255.

enum Pattern : uint8_t { Off, Solid, Blink, Fade };

struct LedState {
  uint8_t pattern;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  uint8_t speed;
};

constexpr size_t STATE_SIZE = 6;
constexpr LedState DEFAULT_STATE{Off, 255, 255, 255, 50, 50};

inline void encodeState(const LedState &state, uint8_t (&bytes)[STATE_SIZE]) {
  bytes[0] = state.pattern;
  bytes[1] = state.red;
  bytes[2] = state.green;
  bytes[3] = state.blue;
  bytes[4] = state.brightness;
  bytes[5] = state.speed;
}

inline bool decodeState(const uint8_t *bytes, size_t size, LedState &state) {
  if (size != STATE_SIZE || bytes[0] > Fade || bytes[4] > 100 || bytes[5] > 100) {
    return false;
  }
  state = {bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]};
  return true;
}

inline uint8_t frameBrightness(const LedState &state, uint32_t elapsedMs) {
  const uint16_t level = state.brightness * LED_BRIGHTNESS_MAX / 100;
  const uint16_t period = 2400 - state.speed * 22;  // 2.4 s to 0.2 s.
  const uint16_t phase = elapsedMs % period;
  switch (state.pattern) {
    case Solid: return level;
    case Blink: return phase < period / 2 ? level : 0;
    case Fade: {
      const uint16_t ramp = phase < period / 2 ? phase : period - phase;
      return level * ramp / (period / 2);
    }
    default: return 0;
  }
}
