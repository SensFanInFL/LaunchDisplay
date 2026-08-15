#include "LaunchSettings.h"

#include <Preferences.h>

namespace
{
constexpr const char *kPrefsNamespace = "launchdisp";
constexpr const char *kKeyConfigured = "configured";
constexpr const char *kKeyApiKey = "api_key";
constexpr const char *kKeyPaidMembership = "paid_membership";
constexpr const char *kKeyApiAccessTier = "api_tier";
constexpr const char *kKeyApiPollFarOut = "api_far_out";
constexpr const char *kKeyApiPollFinalHour = "api_final_hr";
constexpr const char *kKeyApiPollLaunchWindow = "api_launch_win";
constexpr const char *kKeyApiPollPostLaunch = "api_post_launch";
constexpr const char *kKeyLaunchScope = "scope";
constexpr const char *kKeyScopeLocation = "scope_loc";
constexpr const char *kKeyScopeLatitude = "scope_lat";
constexpr const char *kKeyScopeLongitude = "scope_lon";
constexpr const char *kKeyScopeResolved = "scope_resolved";
constexpr const char *kKeyScopeRadius = "scope_radius";
constexpr const char *kKeyScopeUnits = "scope_units";
constexpr const char *kKeyCountry = "country";
constexpr const char *kKeyTimeZone = "timezone";
constexpr const char *kKeyPostLaunch = "post_launch";
constexpr const char *kKeySleepEnabled = "sleep_enabled";
constexpr const char *kKeySleepStart = "sleep_start";
constexpr const char *kKeySleepEnd = "sleep_end";

Preferences prefs;

String readString(const char *key, const char *fallback)
{
  if (!prefs.isKey(key))
  {
    return fallback;
  }

  return prefs.getString(key, fallback);
}
} // namespace

LaunchSettings LaunchSettingsStore::Load()
{
  LaunchSettings settings;
  settings.configured = false;
  settings.apiKey = "";
  settings.paidMembership = false;
  settings.apiAccessTier = "";
  settings.apiPollFarOutMinutes = "15";
  settings.apiPollFinalHourMinutes = "15";
  settings.apiPollLaunchWindowMinutes = "1";
  settings.apiPollPostLaunchMinutes = "30";
  settings.launchScope = "worldwide";
  settings.scopeLocation = "";
  settings.scopeLatitude = 0.0f;
  settings.scopeLongitude = 0.0f;
  settings.scopeResolved = false;
  settings.scopeRadius = "250";
  settings.scopeUnits = "miles";
  settings.country = "United States";
  settings.timeZone = "America/Chicago";
  settings.postLaunchPeriod = "1 hour";
  settings.sleepEnabled = false;
  settings.sleepStart = "22:00";
  settings.sleepEnd = "07:00";

  if (!prefs.begin(kPrefsNamespace, true))
  {
    return settings;
  }

  settings.configured = prefs.getBool(kKeyConfigured, false);
  settings.apiKey = readString(kKeyApiKey, "");
  settings.paidMembership = prefs.getBool(kKeyPaidMembership, false);
  settings.apiAccessTier = readString(kKeyApiAccessTier, "");
  if (settings.apiAccessTier == "free")
  {
    settings.apiAccessTier = "";
  }
  if (!settings.configured)
  {
    settings.paidMembership = false;
    settings.apiAccessTier = "";
  }
  settings.apiPollFarOutMinutes = readString(kKeyApiPollFarOut, settings.apiPollFarOutMinutes.c_str());
  settings.apiPollFinalHourMinutes = readString(kKeyApiPollFinalHour, settings.apiPollFinalHourMinutes.c_str());
  settings.apiPollLaunchWindowMinutes = readString(kKeyApiPollLaunchWindow, settings.apiPollLaunchWindowMinutes.c_str());
  settings.apiPollPostLaunchMinutes = readString(kKeyApiPollPostLaunch, settings.apiPollPostLaunchMinutes.c_str());
  settings.launchScope = readString(kKeyLaunchScope, settings.launchScope.c_str());
  settings.scopeLocation = readString(kKeyScopeLocation, settings.scopeLocation.c_str());
  settings.scopeLatitude = prefs.isKey(kKeyScopeLatitude) ? prefs.getFloat(kKeyScopeLatitude, settings.scopeLatitude) : settings.scopeLatitude;
  settings.scopeLongitude = prefs.isKey(kKeyScopeLongitude) ? prefs.getFloat(kKeyScopeLongitude, settings.scopeLongitude) : settings.scopeLongitude;
  settings.scopeResolved = prefs.isKey(kKeyScopeResolved) ? prefs.getBool(kKeyScopeResolved, settings.scopeResolved) : settings.scopeResolved;
  settings.scopeRadius = readString(kKeyScopeRadius, settings.scopeRadius.c_str());
  settings.scopeUnits = readString(kKeyScopeUnits, settings.scopeUnits.c_str());
  settings.country = readString(kKeyCountry, settings.country.c_str());
  settings.timeZone = readString(kKeyTimeZone, settings.timeZone.c_str());
  settings.postLaunchPeriod = readString(kKeyPostLaunch, settings.postLaunchPeriod.c_str());
  settings.sleepEnabled = prefs.getBool(kKeySleepEnabled, settings.sleepEnabled);
  settings.sleepStart = readString(kKeySleepStart, settings.sleepStart.c_str());
  settings.sleepEnd = readString(kKeySleepEnd, settings.sleepEnd.c_str());

  prefs.end();
  return settings;
}

void LaunchSettingsStore::Save(const LaunchSettings &settings)
{
  if (!prefs.begin(kPrefsNamespace, false))
  {
    return;
  }

  prefs.putBool(kKeyConfigured, settings.configured);
  prefs.putString(kKeyApiKey, settings.apiKey);
  prefs.putBool(kKeyPaidMembership, settings.paidMembership);
  prefs.putString(kKeyApiAccessTier, settings.apiAccessTier);
  prefs.putString(kKeyApiPollFarOut, settings.apiPollFarOutMinutes);
  prefs.putString(kKeyApiPollFinalHour, settings.apiPollFinalHourMinutes);
  prefs.putString(kKeyApiPollLaunchWindow, settings.apiPollLaunchWindowMinutes);
  prefs.putString(kKeyApiPollPostLaunch, settings.apiPollPostLaunchMinutes);
  prefs.putString(kKeyLaunchScope, settings.launchScope);
  prefs.putString(kKeyScopeLocation, settings.scopeLocation);
  prefs.putFloat(kKeyScopeLatitude, settings.scopeLatitude);
  prefs.putFloat(kKeyScopeLongitude, settings.scopeLongitude);
  prefs.putBool(kKeyScopeResolved, settings.scopeResolved);
  prefs.putString(kKeyScopeRadius, settings.scopeRadius);
  prefs.putString(kKeyScopeUnits, settings.scopeUnits);
  prefs.putString(kKeyCountry, settings.country);
  prefs.putString(kKeyTimeZone, settings.timeZone);
  prefs.putString(kKeyPostLaunch, settings.postLaunchPeriod);
  prefs.putBool(kKeySleepEnabled, settings.sleepEnabled);
  prefs.putString(kKeySleepStart, settings.sleepStart);
  prefs.putString(kKeySleepEnd, settings.sleepEnd);
  prefs.end();
}

void LaunchSettingsStore::Clear()
{
  if (!prefs.begin(kPrefsNamespace, false))
  {
    return;
  }

  prefs.clear();
  prefs.end();
}
