// Host C++17 test: exercises the production state, hue and frame-rendering code.
#include "../firmware/AtomNeoPixel/LedRenderer.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>

struct FakeStrip {
  uint32_t pixels[LED_COUNT_MAX]{};
  unsigned shows = 0;
  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
  }
  // Preserve hue in the test double so assertions can inspect its exact angle.
  static uint32_t ColorHSV(uint16_t h, uint8_t, uint8_t v) {
    return v ? 0x80000000u | (uint32_t(v) << 16) | h : 0;
  }
  uint32_t getPixelColor(uint8_t i) const { return pixels[i]; }
  void setPixelColor(uint8_t i, uint32_t color) { pixels[i] = color; }
  void show() { ++shows; }
};

void checkRejected(const uint8_t *bytes, size_t size) {
  LedState state = DEFAULT_STATE;
  uint8_t before[STATE_SIZE], after[STATE_SIZE];
  encodeState(state, before);
  assert(!decodeState(bytes, size, state));
  encodeState(state, after);
  assert(std::memcmp(before, after, STATE_SIZE) == 0);
}

int main() {
  static_assert(DEFAULT_STATE.hueSpan == 360, "Initial angle");
  static_assert(rainbowPixelHue(0, 1, 4, 180) == 10922, "60 degrees");
  static_assert(rainbowPixelHue(0, 2, 4, 180) == 21845, "120 degrees");
  static_assert(rainbowPixelHue(0, 3, 4, 180) == 32768, "180 degrees");
  static_assert(rainbowPixelHue(12345, 14, 15, 360) == 12345, "Same endpoints");
  for (unsigned span = 0; span <= 360; span += 10) {
    for (uint8_t count = 1; count <= LED_COUNT_MAX; ++count) {
      for (uint8_t pattern = Off; pattern <= Rainbow; ++pattern) {
        LedState source{pattern, 37, 128, 255, 100, 60, count, uint16_t(span)};
        uint8_t bytes[STATE_SIZE], roundTrip[STATE_SIZE];
        encodeState(source, bytes);
        LedState decoded = DEFAULT_STATE;
        assert(decodeState(bytes, sizeof(bytes), decoded));
        encodeState(decoded, roundTrip);
        assert(std::memcmp(bytes, roundTrip, STATE_SIZE) == 0);
      }
      for (uint8_t speed = 1; speed <= 60; ++speed) {
        for (uint32_t elapsed : {0u, 250u, 500u, 1000u, 30000u, 59999u, 60000u, 0xffffffffu}) {
          const uint16_t base = rainbowHue(elapsed, speed);
          const uint16_t expectedBase = uint64_t(elapsed) * speed % 60000 * 65536 / 60000;
          assert(base == expectedBase);
          for (uint8_t index = 0; index < count; ++index) {
            const uint64_t offset = count == 1 ? 0 :
                uint64_t(span) * index * 65536 / (360 * (count - 1));
            assert(rainbowPixelHue(base, index, count, uint16_t(span)) == uint16_t(base + offset));
          }
        }
      }
    }
  }
  uint8_t wire[STATE_SIZE];
  encodeState(DEFAULT_STATE, wire);
  assert(wire[7] == 0x68 && wire[8] == 0x01);  // Little-endian 360.
  checkRejected(nullptr, 0);
  for (size_t length = 1; length <= 12; ++length) {
    if (!supportedStateSize(length)) checkRejected(wire, length);
  }
  for (uint16_t invalid : {uint16_t(1), uint16_t(359), uint16_t(361), uint16_t(65535)}) {
    wire[7] = invalid & 255;
    wire[8] = invalid >> 8;
    checkRejected(wire, STATE_SIZE);
  }
  for (unsigned field : {0u, 4u, 5u, 6u}) {
    encodeState(DEFAULT_STATE, wire);
    wire[field] = 255;
    checkRejected(wire, STATE_SIZE);
  }
  encodeState(DEFAULT_STATE, wire);
  wire[6] = 0;
  checkRejected(wire, STATE_SIZE);
  wire[6] = 15;
  wire[0] = Rainbow;
  for (uint8_t invalid : {uint8_t(0), uint8_t(61), uint8_t(100)}) {
    wire[5] = invalid;
    checkRejected(wire, STATE_SIZE);
  }
  LedState old = DEFAULT_STATE;
  old.hueSpan = 180;
  wire[5] = 30;
  assert(decodeState(wire, PREVIOUS_STATE_SIZE, old));
  assert(old.hueSpan == 180 && old.speed == 30);
  checkRejected(wire, LEGACY_STATE_SIZE);  // Six-byte format had no Rainbow.
  wire[0] = Fade;
  wire[5] = 8;
  old.ledCount = 4;
  assert(decodeState(wire, LEGACY_STATE_SIZE, old));
  assert(old.hueSpan == 180 && old.ledCount == 4 && old.speed == 4);
  wire[5] = 0;
  assert(decodeState(wire, PREVIOUS_STATE_SIZE, old) && old.speed == 1);

  FakeStrip strip;
  LedState state{Rainbow, 255, 255, 255, 100, 60, 4, 0};
  assert(renderLedFrame(strip, state, 0));
  for (unsigned i = 0; i < 4; ++i) assert(strip.pixels[i] == strip.pixels[0]);
  assert(!renderLedFrame(strip, state, 0) && strip.shows == 1);
  const uint32_t first = strip.pixels[0];
  state.hueSpan = 180;
  assert(renderLedFrame(strip, state, 0));  // Only angle changed; first pixel is unchanged.
  assert(strip.pixels[0] == first);
  assert((strip.pixels[1] & 65535) == 10922);
  assert((strip.pixels[2] & 65535) == 21845);
  assert((strip.pixels[3] & 65535) == 32768);
  state.hueSpan = 360;
  assert(renderLedFrame(strip, state, 0));
  assert(strip.pixels[0] == strip.pixels[3]);
  assert(renderLedFrame(strip, state, 250));
  assert((strip.pixels[0] & 65535) == 16384);
  assert(renderLedFrame(strip, state, 1000));
  assert(strip.pixels[0] == first);
  state.ledCount = 1;
  assert(renderLedFrame(strip, state, 0));
  for (unsigned i = 1; i < LED_COUNT_MAX; ++i) assert(strip.pixels[i] == 0);
  state.brightness = 0;
  assert(renderLedFrame(strip, state, 0));
  for (auto pixel : strip.pixels) assert(pixel == 0);

  state = {Solid, 255, 0, 0, 100, 10, 15, 360};
  assert(renderLedFrame(strip, state, 0));
  for (auto pixel : strip.pixels) assert(pixel == 0x200000);
  state.pattern = Blink;
  assert(!renderLedFrame(strip, state, 0));
  assert(renderLedFrame(strip, state, 500));
  for (auto pixel : strip.pixels) assert(pixel == 0);
  state.pattern = Fade;
  assert(renderLedFrame(strip, state, 250));
  for (auto pixel : strip.pixels) assert(pixel == 0x100000);
  state.pattern = Off;
  assert(renderLedFrame(strip, state, 250));
  for (auto pixel : strip.pixels) assert(pixel == 0);
  std::puts("PASS: state formats, angle/period matrix, rendering changes and existing patterns");
}
