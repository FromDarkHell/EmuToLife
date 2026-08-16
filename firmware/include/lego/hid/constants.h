#pragma once

#include <Arduino.h>
#include <constants.h>
#include <lego/constants.h>

constexpr int NUM_PADS = 0x03;

using ToypadPlatform = ToysToLifeLib::ToypadPlatform;
using PadColor = ToysToLifeLib::PadColor;
using PadLocation = ToysToLifeLib::PadLocation;

using PadDisplayMode = LEGODimensions::PadDisplayMode;
using PadFadeState = LEGODimensions::PadFadeState;
using PadFlashState = LEGODimensions::PadFlashState;