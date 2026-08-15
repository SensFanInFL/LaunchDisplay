#pragma once

#include <Arduino.h>

struct LaunchSettings
{
  bool configured;
  String apiKey;
  bool paidMembership;
  String apiAccessTier;
  String apiPollFarOutMinutes;
  String apiPollFinalHourMinutes;
  String apiPollLaunchWindowMinutes;
  String apiPollPostLaunchMinutes;
  String launchScope;
  String scopeLocation;
  float scopeLatitude;
  float scopeLongitude;
  bool scopeResolved;
  String scopeRadius;
  String scopeUnits;
  String country;
  String timeZone;
  String postLaunchPeriod;
  bool sleepEnabled;
  String sleepStart;
  String sleepEnd;
};

namespace LaunchSettingsStore
{
LaunchSettings Load();
void Save(const LaunchSettings &settings);
void Clear();
} // namespace LaunchSettingsStore
