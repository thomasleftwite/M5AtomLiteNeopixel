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
  uint8_t speed;  // Cycles per minute for Rainbow, tenths of a cycle per second otherwise.
  uint8_t ledCount;
  uint16_t hueSpan;  // Inclusive first-to-last hue angle, 0-360 degrees in steps of 10.
};

constexpr size_t LEGACY_STATE_SIZE = 6;
constexpr size_t PREVIOUS_STATE_SIZE = 7;
constexpr size_t STATE_SIZE = 9;
constexpr LedState DEFAULT_STATE{Off, 255, 255, 255, 50, 8, LED_COUNT_MAX, 360};

constexpr bool supportedStateSize(size_t size) {
  return size == LEGACY_STATE_SIZE || size == PREVIOUS_STATE_SIZE || size == STATE_SIZE;
}

inline void encodeState(const LedState &state, uint8_t (&bytes)[STATE_SIZE]) {
  bytes[0] = state.pattern;
  bytes[1] = state.red;
  bytes[2] = state.green;
  bytes[3] = state.blue;
  bytes[4] = state.brightness;
  bytes[5] = state.speed;
  bytes[6] = state.ledCount;
  bytes[7] = state.hueSpan & 255;
  bytes[8] = state.hueSpan >> 8;
}

inline bool decodeState(const uint8_t *bytes, size_t size, LedState &state) {
  const bool legacy = size == LEGACY_STATE_SIZE;
  if (!supportedStateSize(size) || bytes[0] > (legacy ? Fade : Rainbow) || bytes[4] > 100) {
    return false;
  }
  uint8_t speed = bytes[5];
  const uint8_t ledCount = legacy ? state.ledCount : bytes[6];
  const uint16_t hueSpan = size == STATE_SIZE ? bytes[7] | (uint16_t(bytes[8]) << 8) : state.hueSpan;
  if (ledCount < 1 || ledCount > LED_COUNT_MAX ||
      hueSpan > 360 || hueSpan % 10 != 0 ||
      (bytes[0] == Rainbow ? speed < 1 || speed > 60 : speed > 100)) {
    return false;
  }
  if (legacy) {
    const uint16_t oldPeriod = 2400 - speed * 22;
    speed = (10000 + oldPeriod / 2) / oldPeriod;
  } else if (speed == 0) {
    speed = 1;  // Accept the previous minimum and use the new slowest rate.
  }
  state = {bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], speed, ledCount, hueSpan};
  return true;
}

constexpr uint32_t cyclePhase(uint32_t elapsedMs, uint8_t speed, uint32_t intervalMs) {
  return ((elapsedMs % intervalMs) * speed) % intervalMs;
}

constexpr uint16_t rainbowHue(uint32_t elapsedMs, uint8_t cyclesPerMinute) {
  constexpr uint32_t MS_PER_MINUTE = 60000;
  const uint32_t phase = cyclePhase(elapsedMs, cyclesPerMinute, MS_PER_MINUTE);
  return phase * 65536u / MS_PER_MINUTE;
}

constexpr uint16_t rainbowPixelHue(uint16_t baseHue, uint8_t index, uint8_t count,
                                   uint16_t hueSpan) {
  // Compute before wrapping so 360 degrees makes the two endpoints identical.
  const uint32_t offset = count > 1
      ? uint32_t(hueSpan) * 65536u * index / (360u * (count - 1)) : 0;
  return uint16_t(baseHue + offset);
}

constexpr uint8_t frameBrightness(const LedState &state, uint32_t elapsedMs) {
  constexpr uint16_t SPEED_INTERVAL_MS = 10000;
  constexpr uint16_t HALF_CYCLE = SPEED_INTERVAL_MS / 2;
  const uint16_t level = state.brightness * LED_BRIGHTNESS_MAX / 100;
  const uint16_t phase = cyclePhase(elapsedMs, state.speed, SPEED_INTERVAL_MS);
  switch (state.pattern) {
    case Solid:
    case Rainbow: return level;
    case Blink: return phase < HALF_CYCLE ? level : 0;
    case Fade: {
      const uint16_t ramp = phase < HALF_CYCLE ? phase : SPEED_INTERVAL_MS - phase;
      return level * ramp / HALF_CYCLE;
    }
    default: return 0;
  }
}
