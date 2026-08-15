#pragma once

#include <Arduino.h>

#include "LaunchDisplayApp.h"
#include "LaunchSettings.h"

class LaunchLibraryClient
{
public:
  bool SynchronizeClock(const String &timeZone, String &errorMessage);
  bool ResolveScopeLocation(const String &query, float &latitude, float &longitude, String &resolvedName, String &errorMessage);
  bool RefreshNextLaunch(const LaunchSettings &settings, LaunchPreviewState &preview, String &errorMessage);
};
