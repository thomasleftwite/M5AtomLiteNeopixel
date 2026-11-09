#pragma once

#include "LedState.h"

// Strip supplies the Adafruit NeoPixel color and pixel-buffer operations.
template <typename Strip>
bool renderLedFrame(Strip &strip, const LedState &state, uint32_t elapsedMs) {
  const uint8_t level = frameBrightness(state, elapsedMs);
  const uint16_t baseHue = rainbowHue(elapsedMs, state.speed);
  const uint32_t solid = strip.Color(state.red * level / 255, state.green * level / 255,
                                    state.blue * level / 255);
  bool changed = false;
  for (uint8_t i = 0; i < LED_COUNT_MAX; ++i) {
    const uint32_t color = i >= state.ledCount ? 0 : state.pattern == Rainbow
        ? strip.ColorHSV(rainbowPixelHue(baseHue, i, state.ledCount, state.hueSpan), 255, level)
        : solid;
    if (strip.getPixelColor(i) != color) {
      strip.setPixelColor(i, color);
      changed = true;
    }
  }
  if (changed) strip.show();
  return changed;
}
