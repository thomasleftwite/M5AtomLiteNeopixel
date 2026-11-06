#pragma once

#include "LedState.h"

namespace BleControl {
void begin(const char *name, const LedState &initial);
void openWindow();
void tick();
bool windowOpen();
bool isConnected();
bool takeCommand(LedState &state);
}
