#include <Arduino.h>
#include <array>
#include <time.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>

#include "DisplayConfig.h"
#include "LGFX.h"
#include "LaunchDisplayApp.h"
#include "LaunchLibraryClient.h"
#include "LaunchSettings.h"
#include "WiFiManagerHelpers.h"

static void LoadSleepSchedule(const LaunchSettings &settings);

enum class LaunchScreenMode
{
  Setup,
  Preview,
};

extern LaunchScreenMode screenMode;
extern bool screenNeedsRedraw;
extern LaunchLibraryClient launchLibraryClient;

namespace
{
struct LaunchPollState
{
  LaunchPreviewState preview;
  bool hasPreview = false;
  unsigned long nextPollAtMs = 0;
  std::array<unsigned long, 15> pollHistory{};
  size_t pollHistoryCount = 0;
  String lastError;
};

LaunchPollState launchPollState;
bool pendingScopeVerification = false;
LaunchSettings activeSettings;
bool displaySleeping = false;
int sleepStartMinutes = 22 * 60;
int sleepEndMinutes = 7 * 60;
bool sleepScheduleEnabled = false;

struct SelectOption
{
  const char *value;
  const char *label;
};

constexpr SelectOption kLaunchScopeOptions[] = {
    {"within_distance", "Within a distance"},
    {"country", "Country"},
    {"worldwide", "Worldwide"},
};

constexpr SelectOption kDistanceUnitOptions[] = {
    {"miles", "Miles"},
    {"kilometers", "Kilometers"},
};

constexpr SelectOption kPostLaunchOptions[] = {
    {"15 minutes", "15 minutes"},
    {"30 minutes", "30 minutes"},
    {"1 hour", "1 hour"},
    {"2 hours", "2 hours"},
};

constexpr SelectOption kApiTierOptions[] = {
    {"regular_supporter", "Regular Supporter (45 API calls/hour)"},
    {"advanced_supporter", "Advanced Supporter (210 API calls/hour)"},
    {"premium_supporter", "Premium Supporter (500 API calls/hour)"},
    {"advanced_api_setup", "Advanced API Setup"},
};

constexpr SelectOption kSleepTimeOptions[] = {
    {"21:00", "9:00 PM"},
    {"22:00", "10:00 PM"},
    {"23:00", "11:00 PM"},
    {"00:00", "12:00 AM"},
    {"01:00", "1:00 AM"},
    {"06:00", "6:00 AM"},
    {"07:00", "7:00 AM"},
    {"08:00", "8:00 AM"},
};

constexpr SelectOption kCountryOptions[] = {
    {"United States", "United States"},
    {"Canada", "Canada"},
    {"Mexico", "Mexico"},
    {"United Kingdom", "United Kingdom"},
    {"Brazil", "Brazil"},
    {"Spain", "Spain"},
    {"France", "France"},
    {"Germany", "Germany"},
    {"Italy", "Italy"},
    {"Australia", "Australia"},
    {"Japan", "Japan"},
    {"New Zealand", "New Zealand"},
};

constexpr SelectOption kTimeZoneOptions[] = {
    {"UTC", "UTC"},
    {"America/New_York", "America/New_York"},
    {"America/Chicago", "America/Chicago"},
    {"America/Denver", "America/Denver"},
    {"America/Los_Angeles", "America/Los_Angeles"},
    {"America/Phoenix", "America/Phoenix"},
    {"Europe/London", "Europe/London"},
    {"Europe/Paris", "Europe/Paris"},
    {"Asia/Tokyo", "Asia/Tokyo"},
    {"Australia/Sydney", "Australia/Sydney"},
};

String HtmlEscape(const String &input)
{
  String output;
  output.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i)
  {
    const char c = input[i];
    switch (c)
    {
    case '&':
      output += F("&amp;");
      break;
    case '<':
      output += F("&lt;");
      break;
    case '>':
      output += F("&gt;");
      break;
    case '"':
      output += F("&quot;");
      break;
    case '\'':
      output += F("&#39;");
      break;
    default:
      output += c;
      break;
    }
  }
  return output;
}

String RenderSelectOptions(const String &currentValue, const SelectOption *options, size_t optionCount)
{
  String html;
  bool matched = false;
  for (size_t i = 0; i < optionCount; ++i)
  {
    if (currentValue == options[i].value)
    {
      matched = true;
      break;
    }
  }

  if (!matched && currentValue.length())
  {
    html += F("<option value='");
    html += HtmlEscape(currentValue);
    html += F("' selected>");
    html += HtmlEscape(currentValue);
    html += F("</option>");
  }

  for (size_t i = 0; i < optionCount; ++i)
  {
    html += F("<option value='");
    html += HtmlEscape(options[i].value);
    html += F("'");
    if (currentValue == options[i].value)
    {
      html += F(" selected");
    }
    html += F(">");
    html += HtmlEscape(options[i].label);
    html += F("</option>");
  }

  return html;
}

String RenderSelectField(const char *name, const char *label, const String &currentValue, const SelectOption *options, size_t optionCount, const String &hint = String())
{
  String html;
  html += F("<label class='field'>");
  html += F("<span>");
  html += HtmlEscape(label);
  html += F("</span>");
  html += F("<select name='");
  html += HtmlEscape(name);
  html += F("'>");
  html += RenderSelectOptions(currentValue, options, optionCount);
  html += F("</select>");
  if (hint.length())
  {
    html += F("<div class='hint'>");
    html += HtmlEscape(hint);
    html += F("</div>");
  }
  html += F("</label>");
  return html;
}

String RenderTextField(const char *name, const char *label, const String &value, const char *placeholder, const String &hint = String())
{
  String html;
  html += F("<label class='field'>");
  html += F("<span>");
  html += HtmlEscape(label);
  html += F("</span>");
  html += F("<input name='");
  html += HtmlEscape(name);
  html += F("' type='text' value='");
  html += HtmlEscape(value);
  html += F("' placeholder='");
  html += HtmlEscape(placeholder);
  html += F("'>");
  if (hint.length())
  {
    html += F("<div class='hint'>");
    html += HtmlEscape(hint);
    html += F("</div>");
  }
  html += F("</label>");
  return html;
}

String RenderTimeField(const char *name, const char *label, const String &value, const String &hint = String())
{
  String html;
  html += F("<label class='field'>");
  html += F("<span>");
  html += HtmlEscape(label);
  html += F("</span>");
  html += F("<input name='");
  html += HtmlEscape(name);
  html += F("' type='time' value='");
  html += HtmlEscape(value);
  html += F("'>");
  if (hint.length())
  {
    html += F("<div class='hint'>");
    html += HtmlEscape(hint);
    html += F("</div>");
  }
  html += F("</label>");
  return html;
}

String RenderNumberField(const char *name, const char *label, const String &value, const char *minValue, const char *maxValue, const String &hint = String())
{
  String html;
  html += F("<label class='field'>");
  html += F("<span>");
  html += HtmlEscape(label);
  html += F("</span>");
  html += F("<input name='");
  html += HtmlEscape(name);
  html += F("' type='number' min='");
  html += HtmlEscape(minValue);
  html += F("' max='");
  html += HtmlEscape(maxValue);
  html += F("' value='");
  html += HtmlEscape(value);
  html += F("'>");
  if (hint.length())
  {
    html += F("<div class='hint'>");
    html += HtmlEscape(hint);
    html += F("</div>");
  }
  html += F("</label>");
  return html;
}

String RenderCheckboxField(const char *name, const char *label, bool checked, const String &hint = String())
{
  String html;
  html += F("<label class='field inline'>");
  html += F("<span>");
  html += HtmlEscape(label);
  html += F("</span><input name='");
  html += HtmlEscape(name);
  html += F("' type='checkbox'");
  if (checked)
  {
    html += F(" checked");
  }
  html += F("></label>");
  if (hint.length())
  {
    html += F("<div class='hint'>");
    html += HtmlEscape(hint);
    html += F("</div>");
  }
  return html;
}

String BuildSetupPage(const LaunchSettings &settings, const String &wifiSsid, const String &deviceIp, bool saved = false, const String &bannerMessage = String(), bool bannerIsError = false)
{
  const String setupUrl = String("http://") + deviceIp + ":8080/launch-setup";
  const bool wifiReady = wifiSsid.length() > 0 && deviceIp.length() > 0 && deviceIp != "0.0.0.0";
  const bool launchConfigured = settings.configured;

  String html;
  html.reserve(12000);
  html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>LaunchDisplay Setup</title><style>");
  html += F("body{margin:0;padding:18px;background:#081017;color:#d6ffe1;font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;}");
  html += F("main{max-width:860px;margin:0 auto;border:1px solid #30543f;background:#0d1512;padding:18px;border-radius:12px;}");
  html += F("h1{margin:0 0 10px;font-size:2rem;color:#7cff9a;}");
  html += F(".section{border:1px solid #315844;background:#09100d;padding:14px;border-radius:10px;margin:14px 0;}");
  html += F(".section.green{border-color:#4b915f;}");
  html += F(".section.orange{border-color:#9a6b32;}");
  html += F(".section.red{border-color:#a44;}");
  html += F(".section-title{font-weight:700;color:#7cff9a;margin-bottom:10px;}");
  html += F(".section.orange .section-title{color:#ffb04c;}");
  html += F(".section.red .section-title{color:#ff8b8b;}");
  html += F(".field{display:flex;flex-direction:column;gap:6px;margin:12px 0;}");
  html += F(".field.inline{flex-direction:row;align-items:center;justify-content:space-between;gap:12px;}");
  html += F(".field span{font-weight:600;color:#d6ffe1;}");
  html += F(".field input,.field select{width:100%;box-sizing:border-box;background:#081017;color:#d6ffe1;border:1px solid #4d7d5b;padding:10px 12px;border-radius:8px;font-size:1rem;}");
  html += F(".field.dimmed{opacity:0.45;}");
  html += F(".field.active span{color:#7cff9a;}");
  html += F(".field.inactive span{color:#ffb04c;}");
  html += F(".hidden{display:none;}");
  html += F(".subsection{border:1px dashed #315844;background:#081017;padding:12px;border-radius:8px;margin:12px 0;}");
  html += F(".subsection-title{font-weight:700;color:#7cff9a;margin-bottom:8px;}");
  html += F(".hint{font-size:0.88rem;color:#90b9a3;line-height:1.35;}");
  html += F(".status{display:grid;grid-template-columns:1fr;gap:8px;font-size:0.95rem;color:#d6ffe1;}");
  html += F(".button-row{display:flex;flex-wrap:wrap;gap:10px;align-items:center;}");
  html += F(".button{display:inline-block;text-decoration:none;padding:10px 14px;border-radius:8px;border:1px solid #4d7d5b;background:#1d3f2d;color:#d6ffe1;font-weight:700;}");
  html += F(".button.danger{border-color:#a44;background:#432;}");
  html += F(".button.save{background:#7cff9a;color:#081017;border-color:#7cff9a;}");
  html += F(".divider{height:1px;background:#315844;margin:14px 0;}");
  html += F(".note{font-size:0.9rem;color:#90b9a3;line-height:1.4;}");
  html += F("code{background:#081017;padding:2px 6px;border-radius:6px;border:1px solid #315844;}");
  html += F("</style></head><body><main>");
  html += F("<h1>LaunchDisplay Setup</h1>");

  if (bannerMessage.length())
  {
    html += F("<div class='section ");
    html += bannerIsError ? F("red") : F("green");
    html += F("'><div class='section-title'>");
    html += bannerIsError ? F("Needs attention") : F("Verified");
    html += F("</div><div class='note'>");
    html += HtmlEscape(bannerMessage);
    html += F("</div></div>");
  }
  else if (saved)
  {
    html += F("<div class='section green'><div class='section-title'>Saved</div><div class='note'>Launch settings were saved successfully.</div></div>");
  }

  html += F("<div class='button-row'><a href='http://");
  html += HtmlEscape(deviceIp);
  html += F(":8080/' class='button' style='background:#1d3f2d;color:#d6ffe1;border:1px solid #4d7d5b'>Back To Main Menu</a></div>");

  html += F("<div class='section ");
  html += wifiReady ? F("green") : F("orange");
  html += F("'><div class='section-title'>Current connection</div><div class='status'>");
  html += F("<div><strong>SSID:</strong> ");
  html += wifiSsid.length() ? HtmlEscape(wifiSsid) : F("Not connected");
  html += F("</div><div><strong>Device IP:</strong> ");
  html += HtmlEscape(deviceIp);
  html += F("</div><div><strong>Setup URL:</strong> <code>");
  html += HtmlEscape(setupUrl);
  html += F("</code></div></div></div>");

  html += F("<form action='/launch-save' method='post' autocomplete='off'>");
  html += F("<div class='section ");
  html += settings.paidMembership ? F("green") : F("orange");
  html += F("' id='api-access-section'><div class='section-title'>API access</div>");
  html += F("<label class='field inline'><span>I have a paid Space Devs membership</span><input name='paid_membership' id='paid-membership' type='checkbox' autocomplete='off'");
  if (settings.paidMembership)
  {
    html += F(" checked");
  }
  html += F("></label>");
  html += F("<label class='field' id='api-tier-field'>");
  html += F("<span>Membership tier</span>");
  html += F("<select name='api_tier' id='api-tier'>");
  html += RenderSelectOptions(settings.apiAccessTier, kApiTierOptions, sizeof(kApiTierOptions) / sizeof(kApiTierOptions[0]));
  html += F("</select>");
  html += F("<div class='hint'>Regular is the simplest path. Advanced API Setup is only for people who want to tune polling themselves.</div>");
  html += F("</label>");
  html += F("<label class='field' id='api-key-field'>");
  html += F("<span>API key</span>");
  html += F("<input name='api_key' id='api-key' type='text' value='");
  html += HtmlEscape(settings.apiKey);
  html += F("' placeholder='Paste your Space Devs API key'>");
  html += F("<div class='hint'>Shown only when paid membership is enabled.</div>");
  html += F("</label>");
  html += F("<div class='subsection hidden' id='advanced-api-section'><div class='subsection-title'>Advanced API setup</div>");
  html += RenderNumberField("api_poll_far_out", "Polling frequency more than 1 hour out (minutes)", settings.apiPollFarOutMinutes, "1", "60", "Suggested starting point: 15 minutes.");
  html += RenderNumberField("api_poll_final_hour", "Polling frequency during the final hour (minutes)", settings.apiPollFinalHourMinutes, "1", "30", "Suggested starting point: 5-15 minutes.");
  html += RenderNumberField("api_poll_launch_window", "Polling frequency during the last five minutes before launch (minutes)", settings.apiPollLaunchWindowMinutes, "1", "10", "Suggested starting point: 1 minute.");
  html += RenderNumberField("api_poll_post_launch", "Polling frequency during the five minutes after launch (minutes)", settings.apiPollPostLaunchMinutes, "1", "60", "Suggested starting point: 5-30 minutes.");
  html += F("<div class='hint'>This section is for manual tuning later. It will be used by the runtime once the paid-tier behavior is wired in.</div></div>");
  html += F("<div class='section ");
  html += launchConfigured ? F("green") : F("orange");
  html += F("'><div class='section-title'>Launch settings</div>");
  html += RenderSelectField("launch_scope", "Launch scope", settings.launchScope, kLaunchScopeOptions, sizeof(kLaunchScopeOptions) / sizeof(kLaunchScopeOptions[0]), "Choose Within a distance, Country, or Worldwide.");
  html += F("<label class='field' id='country-field'>");
  html += F("<span>Country</span>");
  html += F("<select name='country' id='country-select'>");
  html += RenderSelectOptions(settings.country, kCountryOptions, sizeof(kCountryOptions) / sizeof(kCountryOptions[0]));
  html += F("</select>");
  html += F("<div class='hint' id='country-hint'>Use this when Launch scope is Country. Distance does not apply here.</div>");
  html += F("</label>");
  html += F("<label class='field' id='location-field'>");
  html += F("<span>LaunchDisplay Location</span>");
  html += F("<input name='scope_location' id='scope-location' type='text' value='");
  html += HtmlEscape(settings.scopeLocation);
  html += F("' placeholder='Starbase, Texas'>");
  html += F("<div class='hint' id='location-hint'>Enter a city name, not a street address or coordinates. We verify this before saving within-distance scope.</div>");
  html += F("</label>");
  html += F("<label class='field' id='distance-units-field'>");
  html += F("<span>Distance units</span>");
  html += F("<select name='scope_units' id='scope-units'>");
  html += RenderSelectOptions(settings.scopeUnits, kDistanceUnitOptions, sizeof(kDistanceUnitOptions) / sizeof(kDistanceUnitOptions[0]));
  html += F("</select>");
  html += F("<div class='hint' id='distance-units-hint'>Used only when Launch scope is Within a distance.</div>");
  html += F("</label>");
  html += F("<label class='field' id='distance-field'>");
  html += F("<span>Distance</span>");
  html += F("<input name='scope_radius' id='scope-radius' type='number' min='1' max='500' value='");
  html += HtmlEscape(settings.scopeRadius);
  html += F("' placeholder='250'>");
  html += F("<div class='hint' id='distance-hint'>Not used for Country or Worldwide.</div>");
  html += F("</label>");
  html += RenderSelectField("timezone", "Time Zone", settings.timeZone, kTimeZoneOptions, sizeof(kTimeZoneOptions) / sizeof(kTimeZoneOptions[0]), "Used for launch times and countdown display.");
  html += RenderSelectField("post_launch", "Post-launch rollover", settings.postLaunchPeriod, kPostLaunchOptions, sizeof(kPostLaunchOptions) / sizeof(kPostLaunchOptions[0]), "How long to keep showing T+ after launch.");
  html += F("<div class='section ");
  html += settings.sleepEnabled ? F("green") : F("orange");
  html += F("'><div class='section-title'>Sleep schedule</div>");
  html += F("<label class='field'><span>Enable schedule</span><input name='sleep_enabled' type='checkbox'");
  if (settings.sleepEnabled)
  {
    html += F(" checked");
  }
  html += F("></label>");
  html += RenderSelectField("sleep_start", "Sleep starts", settings.sleepStart, kSleepTimeOptions, sizeof(kSleepTimeOptions) / sizeof(kSleepTimeOptions[0]), "Display turns off at the start time.");
  html += RenderSelectField("sleep_end", "Sleep ends", settings.sleepEnd, kSleepTimeOptions, sizeof(kSleepTimeOptions) / sizeof(kSleepTimeOptions[0]), "Display wakes back up at the end time.");
  html += F("</div>");
  html += F("<div class='button-row'><button type='submit' class='button save'>Save</button></div>");
  html += F("</div></form>");

  html += F("<script>");
  html += F("(() => {");
  html += F("const paidMembership = document.getElementById('paid-membership');");
  html += F("const apiAccessSection = document.getElementById('api-access-section');");
  html += F("const apiTierField = document.getElementById('api-tier-field');");
  html += F("const apiTier = document.getElementById('api-tier');");
  html += F("const apiKeyField = document.getElementById('api-key-field');");
  html += F("const apiKeyInput = document.getElementById('api-key');");
  html += F("const advancedApiSection = document.getElementById('advanced-api-section');");
  html += F("const scope = document.querySelector(\"select[name='launch_scope']\");");
  html += F("const countryField = document.getElementById('country-field');");
  html += F("const countryHint = document.getElementById('country-hint');");
  html += F("const countrySelect = document.getElementById('country-select');");
  html += F("const locationField = document.getElementById('location-field');");
  html += F("const locationHint = document.getElementById('location-hint');");
  html += F("const locationInput = document.getElementById('scope-location');");
  html += F("const field = document.getElementById('distance-field');");
  html += F("const unitsField = document.getElementById('distance-units-field');");
  html += F("const input = document.getElementById('scope-radius');");
  html += F("const unitsInput = document.getElementById('scope-units');");
  html += F("const hint = document.getElementById('distance-hint');");
  html += F("const unitsHint = document.getElementById('distance-units-hint');");
  html += F("function syncApiState(){");
  html += F("const paid = paidMembership && paidMembership.checked;");
  html += F("const advanced = apiTier && apiTier.value === 'advanced_api_setup';");
  html += F("apiAccessSection.classList.toggle('green', paid);");
  html += F("apiAccessSection.classList.toggle('orange', !paid);");
  html += F("apiTierField.classList.toggle('hidden', !paid);");
  html += F("apiKeyField.classList.toggle('hidden', !paid);");
  html += F("advancedApiSection.classList.toggle('hidden', !(paid && advanced));");
  html += F("apiKeyInput.disabled = !paid;");
  html += F("apiTier.disabled = !paid;");
  html += F("}");
  html += F("function syncDistanceState(){");
  html += F("const active = scope && scope.value === 'within_distance';");
  html += F("const countryMode = scope && scope.value === 'country';");
  html += F("const worldwideMode = scope && scope.value === 'worldwide';");
  html += F("field.classList.toggle('dimmed', !active);");
  html += F("unitsField.classList.toggle('dimmed', !active);");
  html += F("locationField.classList.toggle('dimmed', !active);");
  html += F("countryField.classList.toggle('dimmed', !countryMode);");
  html += F("countryField.classList.toggle('active', countryMode);");
  html += F("countryField.classList.toggle('inactive', !countryMode);");
  html += F("input.disabled = !active;");
  html += F("unitsInput.disabled = !active;");
  html += F("locationInput.disabled = !active;");
  html += F("countrySelect.disabled = !countryMode;");
  html += F("countrySelect.classList.toggle('dimmed', !countryMode);");
  html += F("hint.textContent = active ? 'Whole numbers only.' : 'Not used for Country or Worldwide.';");
  html += F("unitsHint.textContent = active ? 'Use Miles or Kilometers for this launch scope.' : 'Not used for Country or Worldwide.';");
  html += F("countryHint.textContent = countryMode ? 'Use this when Launch scope is Country. Distance does not apply here.' : 'Not used for Worldwide or Within a distance.';");
  html += F("countryHint.style.color = countryMode ? '#90b9a3' : '#ffb04c';");
  html += F("locationHint.textContent = active ? 'Enter a city name, not a street address or coordinates. Required only when Launch scope is Within a distance.' : 'Enter a city name, not a street address or coordinates.';");
  html += F("locationHint.style.color = active ? '#90b9a3' : '#ffb04c';");
  html += F("if (worldwideMode) { countryHint.textContent = 'Not used for Worldwide or Within a distance.'; }");
  html += F("}");
  html += F("if (paidMembership) { paidMembership.addEventListener('change', syncApiState); }");
  html += F("if (apiTier) { apiTier.addEventListener('change', syncApiState); }");
  html += F("if (scope) { scope.addEventListener('change', syncDistanceState); }");
  html += F("syncApiState(); syncDistanceState();");
  html += F("})();");
  html += F("</script>");

  html += F("</main></body></html>");
  return html;
}

String BuildMenuPage(const LaunchSettings &settings, const String &wifiSsid, const String &deviceIp)
{
  const bool launchConfigured = settings.configured;
  const String setupLabel = launchConfigured ? "Launch setup" : "Continue setup";
  const String setupButtonColor = launchConfigured ? "#7cff9a;color:#081017;border:1px solid #7cff9a" : "#ffb04c;color:#081017;border:1px solid #ffb04c";

  String html;
  html.reserve(5000);
  html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>LaunchDisplay Setup</title><style>");
  html += F("body{margin:0;padding:18px;background:#081017;color:#d6ffe1;font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;}");
  html += F("main{max-width:860px;margin:0 auto;border:1px solid #30543f;background:#0d1512;padding:18px;border-radius:12px;}");
  html += F("h1{margin:0 0 10px;font-size:2rem;color:#7cff9a;}");
  html += F(".section{border:1px solid #315844;background:#09100d;padding:14px;border-radius:10px;margin:14px 0;}");
  html += F(".section.green{border-color:#4b915f;}");
  html += F(".section.orange{border-color:#9a6b32;}");
  html += F(".section.red{border-color:#a44;}");
  html += F(".section-title{font-weight:700;color:#7cff9a;margin-bottom:10px;}");
  html += F(".section.orange .section-title{color:#ffb04c;}");
  html += F(".section.red .section-title{color:#ff8b8b;}");
  html += F(".status{display:grid;grid-template-columns:1fr;gap:8px;font-size:0.95rem;color:#d6ffe1;}");
  html += F(".button-row{display:flex;flex-direction:column;gap:10px;}");
  html += F(".button{display:inline-block;text-decoration:none;padding:12px 14px;border-radius:8px;font-weight:700;text-align:center;}");
  html += F("</style></head><body><main>");
  html += F("<h1>LaunchDisplay Setup</h1>");
  html += F("<div class='section ");
  html += launchConfigured ? F("green") : F("orange");
  html += F("'><div class='section-title'>Current connection</div><div class='status'>");
  html += F("<div><strong>SSID:</strong> ");
  html += wifiSsid.length() ? HtmlEscape(wifiSsid) : F("Not connected");
  html += F("</div><div><strong>Device IP:</strong> ");
  html += HtmlEscape(deviceIp);
  html += F("</div></div></div>");
  html += F("<div class='section ");
  html += launchConfigured ? F("green") : F("orange");
  html += F("'><div class='section-title'>Menu</div><div class='button-row'>");
  html += F("<a href='/launch-setup' class='button' style='background:");
  html += setupButtonColor;
  html += F("'>");
  html += setupLabel;
  html += F("</a>");
  html += F("<a href='/wifi-reset' class='button' style='background:#a44;color:#ffd5d5;border:1px solid #ff8b8b'>Wi-Fi reset</a>");
  html += F("<a href='/factory-reset' class='button' style='background:#5a2222;color:#ffd5d5;border:1px solid #ff8b8b'>Factory reset</a>");
  html += F("</div></div>");
  html += F("<div class='section'><div class='section-title'>Setup URL</div><div class='status'><div><code>http://");
  html += HtmlEscape(deviceIp);
  html += F(":8080</code></div></div></div>");
  html += F("</main></body></html>");
  return html;
}

String BuildResetPage(const char *title, const char *description, const char *actionPath, const char *buttonLabel)
{
  String html;
  html.reserve(2500);
  html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>LaunchDisplay ");
  html += HtmlEscape(title);
  html += F("</title><style>body{margin:0;padding:18px;background:#081017;color:#d6ffe1;font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;}main{max-width:700px;margin:0 auto;border:1px solid #a44;background:#180d0d;padding:18px;border-radius:12px;}h1{margin:0 0 10px;color:#ff8b8b;}p{line-height:1.4;color:#e8b6b6;}a,button{display:inline-block;text-decoration:none;padding:10px 14px;border-radius:8px;border:1px solid #a44;background:#452020;color:#ffd5d5;font-weight:700;font-size:1rem;}button{cursor:pointer;}form{margin-top:16px;} .danger{border-color:#ff8b8b;background:#5a2222;}</style></head><body><main>");
  html += F("<h1>LaunchDisplay ");
  html += HtmlEscape(title);
  html += F("</h1><p>");
  html += HtmlEscape(description);
  html += F("</p><p><a href='/'>Back To Main Menu</a></p><form action='");
  html += HtmlEscape(actionPath);
  html += F("' method='post'><button class='danger' type='submit'>");
  html += HtmlEscape(buttonLabel);
  html += F("</button></form></main></body></html>");
  return html;
}

static bool SaveLaunchSettingsFromPortal(WebServer &server, String &message, bool &messageIsError, bool &verifyLater)
{
  LaunchSettings settings = LaunchSettingsStore::Load();
  settings.apiKey = server.arg("api_key");
  settings.paidMembership = server.hasArg("paid_membership");
  settings.apiAccessTier = settings.paidMembership ? server.arg("api_tier") : "";
  settings.apiPollFarOutMinutes = server.arg("api_poll_far_out");
  settings.apiPollFinalHourMinutes = server.arg("api_poll_final_hour");
  settings.apiPollLaunchWindowMinutes = server.arg("api_poll_launch_window");
  settings.apiPollPostLaunchMinutes = server.arg("api_poll_post_launch");
  settings.launchScope = server.arg("launch_scope");
  settings.country = server.arg("country");
  settings.scopeLocation = server.arg("scope_location");
  settings.scopeRadius = server.arg("scope_radius");
  settings.scopeUnits = server.arg("scope_units");
  settings.timeZone = server.arg("timezone");
  settings.postLaunchPeriod = server.arg("post_launch");
  settings.sleepEnabled = server.hasArg("sleep_enabled");
  settings.sleepStart = server.arg("sleep_start");
  settings.sleepEnd = server.arg("sleep_end");
  if (!settings.launchScope.length())
  {
    settings.launchScope = "worldwide";
  }
  settings.scopeResolved = false;
  settings.scopeLatitude = 0.0f;
  settings.scopeLongitude = 0.0f;
  settings.configured = true;

  if (!settings.paidMembership)
  {
    settings.apiKey = "";
    settings.apiAccessTier = "";
  }
  else if (!settings.apiAccessTier.length() || settings.apiAccessTier == "free")
  {
    settings.apiAccessTier = "regular_supporter";
  }

  message = "";
  messageIsError = false;
  verifyLater = false;

  if (settings.launchScope == "within_distance")
  {
    if (settings.scopeLocation.length())
    {
      verifyLater = true;
      message = F("Saved. Verifying location now...");
    }
    else
    {
      message = F("Location is required when Launch scope is Within a distance.");
      messageIsError = true;
    }
  }
  else
  {
    settings.scopeResolved = false;
  }

  LaunchSettingsStore::Save(settings);
  activeSettings = settings;
  LoadSleepSchedule(activeSettings);
  if (!message.length())
  {
    message = F("Launch settings were saved successfully.");
  }

  return true;
}

static void RegisterLaunchSetupRoutes(WebServer &server, WiFiManager &wm)
{
  auto renderSetupPage = [&server](bool saved, const String &bannerMessage = String(), bool bannerIsError = false)
  {
    const LaunchSettings settings = LaunchSettingsStore::Load();
    IPAddress deviceIp = WiFi.localIP();
    if (deviceIp == IPAddress(0, 0, 0, 0))
    {
      deviceIp = WiFi.softAPIP();
    }
    const String wifiSsid = WiFi.SSID();
    const String html = BuildSetupPage(settings, wifiSsid, deviceIp.toString(), saved, bannerMessage, bannerIsError);
    server.send(200, "text/html", html);
  };

  server.on("/", HTTP_GET, [&server]()
            {
    const LaunchSettings settings = LaunchSettingsStore::Load();
    IPAddress deviceIp = WiFi.localIP();
    if (deviceIp == IPAddress(0, 0, 0, 0))
    {
      deviceIp = WiFi.softAPIP();
    }
    const String html = BuildMenuPage(settings, WiFi.SSID(), deviceIp.toString());
    server.send(200, "text/html", html); });

  server.on("/launch-setup", HTTP_GET, [renderSetupPage]()
            {
    renderSetupPage(false); });

  server.on("/launch-save", HTTP_POST, [&server, renderSetupPage]()
            {
    String saveMessage;
    bool saveError = false;
    bool verifyLater = false;
    const bool saved = SaveLaunchSettingsFromPortal(server, saveMessage, saveError, verifyLater);
    if (saved)
    {
      pendingScopeVerification = verifyLater;
      screenMode = verifyLater ? LaunchScreenMode::Setup : LaunchScreenMode::Preview;
      screenNeedsRedraw = true;
      launchPollState.hasPreview = false;
      launchPollState.nextPollAtMs = 0;
    }
    renderSetupPage(saved, saveMessage, saveError); });

  server.on("/wifi-reset", HTTP_GET, [&server]()
            {
    const String html = BuildResetPage("Wi-Fi reset", "Clears only the saved wireless credentials and returns the device to setup mode.", "/wifi-reset-confirm", "Confirm Wi-Fi reset");
    server.send(200, "text/html", html); });

  server.on("/wifi-reset-confirm", HTTP_POST, [&server, &wm]()
            {
    Serial.println("[WIFI] Wi-Fi reset requested.");
    wm.resetSettings();
    WiFi.disconnect(true, true);
    server.send(200, "text/plain", "Wi-Fi reset complete. Rebooting...");
    delay(1000);
    ESP.restart(); });

  server.on("/factory-reset", HTTP_GET, [&server]()
            {
    const String html = BuildResetPage("Factory reset", "Clears all saved configuration and Wi-Fi credentials.", "/factory-reset-confirm", "Confirm factory reset");
    server.send(200, "text/html", html); });

  server.on("/factory-reset-confirm", HTTP_POST, [&server, &wm]()
            {
    Serial.println("[WIFI] Factory reset requested.");
    LaunchSettingsStore::Clear();
    wm.erase(true);
    WiFi.disconnect(true, true);
    server.send(200, "text/plain", "Factory reset complete. Rebooting...");
    delay(1000);
    ESP.restart(); });
}
} // namespace

LGFX tft;
WiFiManager wifiManager;
WebServer launchSetupServer(8080);
LaunchLibraryClient launchLibraryClient;
LaunchScreenMode screenMode = LaunchScreenMode::Setup;
bool screenNeedsRedraw = true;

static void initDisplay()
{
  pinMode(TFT_PIN_BL, OUTPUT);
  digitalWrite(TFT_PIN_BL, TFT_BL_ACTIVE_HIGH ? HIGH : LOW);

  pinMode(TFT_PIN_RST, OUTPUT);
  digitalWrite(TFT_PIN_RST, HIGH);
  delay(20);
  digitalWrite(TFT_PIN_RST, LOW);
  delay(20);
  digitalWrite(TFT_PIN_RST, HIGH);
  delay(120);

  tft.init();
  tft.invertDisplay(true);
  tft.setRotation(0);
  RenderLaunchSplashScreen(tft, "Booting...");
  delay(750);
}

static void SetDisplayBacklightEnabled(bool enabled)
{
  digitalWrite(TFT_PIN_BL, enabled ? (TFT_BL_ACTIVE_HIGH ? HIGH : LOW) : (TFT_BL_ACTIVE_HIGH ? LOW : HIGH));
}

static void renderPortalAssistScreen()
{
  if (!WiFi.isConnected())
  {
    RenderLaunchWiFiSetupScreen(tft, WiFiManagerHelpers::WiFiManagerName);
    return;
  }

  LaunchSetupState setupState;
  setupState.wifiSsid = WiFi.SSID();
  IPAddress deviceIp = WiFi.localIP();
  if (deviceIp == IPAddress(0, 0, 0, 0))
  {
    deviceIp = WiFi.softAPIP();
  }
  setupState.deviceIp = deviceIp.toString();
  setupState.portalUrl = String("http://") + setupState.deviceIp + ":8080/launch-setup";
  setupState.setupHint = "Scan the QR code to open setup.";
  setupState.footerLine1 = "Launch setup lives on port 8080.";
  setupState.footerLine2 = "Connect Wi-Fi first, then open the QR code.";
  RenderLaunchSetupScreen(tft, setupState);
}

static LaunchDisplayStatus BuildCurrentStatus()
{
  LaunchDisplayStatus status;
  status.wifiConnected = WiFi.isConnected();
  status.wifiIp = WiFi.localIP().toString();
  status.wifiSsid = WiFi.SSID();
  return status;
}

static LaunchPreviewState BuildPreviewState()
{
  LaunchPreviewState preview;
  preview.status = BuildCurrentStatus();
  preview.rocket = "FALCON 9";
  preview.missionName = "Starlink Group 10-XX";
  preview.launchSite = "Cape Canaveral SFS";
  preview.launchPad = "SLC-40";
  preview.scheduledDateTime = "August 18 @ 7:42 PM";
  preview.scheduledEpochUtc = 0;
  preview.countdown = "T-02:14:37";
  preview.statusText = "GO";
  preview.liveDataReady = false;
  preview.liveDataError = false;
  preview.rateLimited = false;
  preview.feedStatus = "Waiting for live launch data";
  return preview;
}

static LaunchSetupState BuildSetupState()
{
  LaunchSetupState setupState;
  setupState.wifiSsid = WiFi.SSID();
  setupState.deviceIp = WiFi.localIP().toString();
  if (setupState.deviceIp == "0.0.0.0")
  {
    setupState.deviceIp = WiFi.softAPIP().toString();
  }
  setupState.portalUrl = String("http://") + setupState.deviceIp + ":8080/launch-setup";
  setupState.setupHint = "Scan the QR code to open setup.";
  setupState.footerLine1 = "Launch setup lives on port 8080.";
  setupState.footerLine2 = "Connect Wi-Fi first, then open the QR code.";
  return setupState;
}

static bool ParseClockMinutes(const String &value, int &minutes)
{
  String trimmed = value;
  trimmed.trim();
  if (!trimmed.length())
  {
    return false;
  }

  String upper = trimmed;
  upper.toUpperCase();

  bool hasMeridiem = false;
  bool isPm = false;
  const int amIndex = upper.indexOf("AM");
  const int pmIndex = upper.indexOf("PM");
  if (amIndex >= 0 || pmIndex >= 0)
  {
    hasMeridiem = true;
    isPm = pmIndex >= 0;
    upper.replace("AM", "");
    upper.replace("PM", "");
    upper.trim();
  }

  int hour = -1;
  int minute = 0;
  const int colon = upper.indexOf(':');
  if (colon > 0)
  {
    hour = upper.substring(0, colon).toInt();
    minute = upper.substring(colon + 1).toInt();
  }
  else
  {
    hour = upper.toInt();
    minute = 0;
  }

  if (minute < 0 || minute > 59)
  {
    return false;
  }

  if (hasMeridiem)
  {
    if (hour < 1 || hour > 12)
    {
      return false;
    }

    hour %= 12;
    if (isPm)
    {
      hour += 12;
    }
  }
  else if (hour < 0 || hour > 23)
  {
    return false;
  }

  minutes = hour * 60 + minute;
  return true;
}

static void LoadSleepSchedule(const LaunchSettings &settings)
{
  sleepScheduleEnabled = settings.sleepEnabled;

  int parsedMinutes = 22 * 60;
  sleepStartMinutes = ParseClockMinutes(settings.sleepStart, parsedMinutes) ? parsedMinutes : (22 * 60);

  parsedMinutes = 7 * 60;
  sleepEndMinutes = ParseClockMinutes(settings.sleepEnd, parsedMinutes) ? parsedMinutes : (7 * 60);
}

static bool IsClockWithinSleepWindow(int currentMinutes)
{
  if (!sleepScheduleEnabled)
  {
    return false;
  }

  if (sleepStartMinutes == sleepEndMinutes)
  {
    return false;
  }

  if (sleepStartMinutes < sleepEndMinutes)
  {
    return currentMinutes >= sleepStartMinutes && currentMinutes < sleepEndMinutes;
  }

  return currentMinutes >= sleepStartMinutes || currentMinutes < sleepEndMinutes;
}

static bool IsDisplaySleeping()
{
  if (!sleepScheduleEnabled)
  {
    return false;
  }

  struct tm now = {};
  if (!getLocalTime(&now, 25))
  {
    return false;
  }

  const int currentMinutes = now.tm_hour * 60 + now.tm_min;
  return IsClockWithinSleepWindow(currentMinutes);
}

static void ResolveLaunchScopeIfNeeded(LaunchSettings &settings)
{
  if (settings.launchScope != "within_distance" || settings.scopeResolved || !settings.scopeLocation.length())
  {
    return;
  }

  String resolvedName;
  String resolveError;
  if (launchLibraryClient.ResolveScopeLocation(settings.scopeLocation, settings.scopeLatitude, settings.scopeLongitude, resolvedName, resolveError))
  {
    settings.scopeResolved = true;
    settings.scopeLocation = resolvedName;
    LaunchSettingsStore::Save(settings);
    Serial.print(F("[LL2] restored location resolved: "));
    Serial.println(resolvedName);
  }
  else
  {
    Serial.print(F("[LL2] restored location resolve failed: "));
    Serial.println(resolveError);
  }
}

static void ResolvePendingLaunchScope()
{
  if (!pendingScopeVerification)
  {
    return;
  }

  const LaunchSettings settings = LaunchSettingsStore::Load();
  if (settings.launchScope != "within_distance" || settings.scopeResolved || !settings.scopeLocation.length())
  {
    pendingScopeVerification = false;
    return;
  }

  String resolvedName;
  String resolveError;
  LaunchSettings resolvedSettings = settings;
  if (launchLibraryClient.ResolveScopeLocation(settings.scopeLocation, resolvedSettings.scopeLatitude, resolvedSettings.scopeLongitude, resolvedName, resolveError))
  {
    resolvedSettings.scopeResolved = true;
    resolvedSettings.scopeLocation = resolvedName;
    LaunchSettingsStore::Save(resolvedSettings);
    activeSettings = resolvedSettings;
    pendingScopeVerification = false;
    screenMode = LaunchScreenMode::Preview;
    launchPollState.hasPreview = false;
    launchPollState.nextPollAtMs = 0;
    screenNeedsRedraw = true;
    Serial.print(F("[LL2] pending location resolved: "));
    Serial.println(resolvedName);
    return;
  }

  pendingScopeVerification = false;
  Serial.print(F("[LL2] pending location resolve failed: "));
  Serial.println(resolveError);
}

static void ApplySleepState()
{
  const bool shouldSleep = IsDisplaySleeping();
  if (shouldSleep == displaySleeping)
  {
    return;
  }

  displaySleeping = shouldSleep;
  SetDisplayBacklightEnabled(!displaySleeping);
  Serial.println(displaySleeping ? F("[SLEEP] display off") : F("[SLEEP] display on"));
}

static unsigned long ParsePositiveUnsignedLong(const String &value, unsigned long fallback)
{
  if (!value.length())
  {
    return fallback;
  }

  const unsigned long parsed = static_cast<unsigned long>(value.toInt());
  return parsed > 0 ? parsed : fallback;
}

static bool IsTransientLaunchFetchError(const String &errorMessage)
{
  return errorMessage.indexOf("HTTP -1") >= 0 || errorMessage.indexOf("SSL - The connection indicated an EOF") >= 0 || errorMessage.indexOf("start_ssl_client") >= 0;
}

static unsigned long LaunchPollBudgetPerHour(const LaunchSettings &settings)
{
  if (!settings.paidMembership)
  {
    return 15UL;
  }

  if (settings.apiAccessTier == "advanced_supporter")
  {
    return 210UL;
  }

  if (settings.apiAccessTier == "premium_supporter" || settings.apiAccessTier == "advanced_api_setup")
  {
    return 500UL;
  }

  return 45UL;
}

static String FormatCountdownForEpoch(time_t launchEpochUtc)
{
  const time_t now = time(nullptr);
  if (now < 1700000000)
  {
    return "T-??:??:??";
  }

  long long delta = static_cast<long long>(launchEpochUtc) - static_cast<long long>(now);
  const char sign = delta >= 0 ? '-' : '+';
  if (delta < 0)
  {
    delta = -delta;
  }

  const long long hours = delta / 3600;
  const long long minutes = (delta / 60) % 60;
  const long long seconds = delta % 60;

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "T%c%02lld:%02lld:%02lld", sign, hours, minutes, seconds);
  return String(buffer);
}

static unsigned long DesiredLaunchPollIntervalMs(const LaunchSettings &settings, const LaunchPreviewState &preview)
{
  if (settings.paidMembership && settings.apiAccessTier == "advanced_api_setup")
  {
    const unsigned long farOutMinutes = ParsePositiveUnsignedLong(settings.apiPollFarOutMinutes, 15UL);
    const unsigned long finalHourMinutes = ParsePositiveUnsignedLong(settings.apiPollFinalHourMinutes, 5UL);
    const unsigned long launchWindowMinutes = ParsePositiveUnsignedLong(settings.apiPollLaunchWindowMinutes, 1UL);
    const unsigned long postLaunchMinutes = ParsePositiveUnsignedLong(settings.apiPollPostLaunchMinutes, 5UL);

    if (preview.scheduledEpochUtc <= 0)
    {
      return farOutMinutes * 60UL * 1000UL;
    }

    const time_t nowEpoch = time(nullptr);
    if (nowEpoch < 1700000000)
    {
      return farOutMinutes * 60UL * 1000UL;
    }

    const long long deltaSeconds = static_cast<long long>(preview.scheduledEpochUtc) - static_cast<long long>(nowEpoch);
    if (deltaSeconds > 3600)
    {
      return farOutMinutes * 60UL * 1000UL;
    }
    if (deltaSeconds > 900)
    {
      return finalHourMinutes * 60UL * 1000UL;
    }
    if (deltaSeconds > 300)
    {
      return launchWindowMinutes * 60UL * 1000UL;
    }
    if (deltaSeconds > -300)
    {
      return 60UL * 1000UL;
    }
    if (deltaSeconds > -1800)
    {
      return postLaunchMinutes * 60UL * 1000UL;
    }

    return farOutMinutes * 60UL * 1000UL;
  }

  if (preview.scheduledEpochUtc <= 0)
  {
    return 15UL * 60UL * 1000UL;
  }

  const time_t nowEpoch = time(nullptr);
  if (nowEpoch < 1700000000)
  {
    return 15UL * 60UL * 1000UL;
  }

  const long long deltaSeconds = static_cast<long long>(preview.scheduledEpochUtc) - static_cast<long long>(nowEpoch);
  if (deltaSeconds > 3600)
  {
    return 30UL * 60UL * 1000UL;
  }
  if (deltaSeconds > 900)
  {
    return 15UL * 60UL * 1000UL;
  }
  if (deltaSeconds > 300)
  {
    return 5UL * 60UL * 1000UL;
  }
  if (deltaSeconds > -300)
  {
    return 60UL * 1000UL;
  }
  if (deltaSeconds > -1800)
  {
    return 5UL * 60UL * 1000UL;
  }

  return 30UL * 60UL * 1000UL;
}

static void UpdateLiveCountdown()
{
  if (screenMode != LaunchScreenMode::Preview || !launchPollState.hasPreview || !launchPollState.preview.liveDataReady || launchPollState.preview.scheduledEpochUtc <= 0)
  {
    return;
  }

  const String updatedCountdown = FormatCountdownForEpoch(launchPollState.preview.scheduledEpochUtc);
  if (updatedCountdown != launchPollState.preview.countdown)
  {
    launchPollState.preview.countdown = updatedCountdown;
    UpdateLaunchPreviewCountdown(tft, updatedCountdown);
  }
}

static void RedrawMainTft()
{
  if (screenMode == LaunchScreenMode::Preview)
  {
    if (!launchPollState.hasPreview || (!launchPollState.preview.liveDataReady && !launchPollState.preview.liveDataError))
    {
      RenderLaunchWaitingScreen(tft, "Waiting for launch data");
      return;
    }

    RenderLaunchPreviewScreen(tft, launchPollState.preview);
  }
  else
  {
    RenderLaunchSetupScreen(tft, BuildSetupState());
  }
}

static bool HasLaunchPollBudget(unsigned long nowMs, unsigned long budgetPerHour)
{
  size_t recentCount = 0;
  for (size_t i = 0; i < launchPollState.pollHistoryCount; ++i)
  {
    if ((nowMs - launchPollState.pollHistory[i]) < 3600000UL)
    {
      ++recentCount;
    }
  }
  return recentCount < budgetPerHour;
}

static unsigned long NextLaunchPollBudgetTime(unsigned long nowMs, unsigned long budgetPerHour)
{
  unsigned long earliest = 0;
  bool found = false;
  for (size_t i = 0; i < launchPollState.pollHistoryCount; ++i)
  {
    const unsigned long pollAt = launchPollState.pollHistory[i];
    if ((nowMs - pollAt) < 3600000UL)
    {
      if (!found || pollAt < earliest)
      {
        earliest = pollAt;
        found = true;
      }
    }
  }

  if (!found)
  {
    return nowMs;
  }

  return earliest + 3600000UL;
}

static void RecordLaunchPoll(unsigned long nowMs)
{
  if (launchPollState.pollHistoryCount < launchPollState.pollHistory.size())
  {
    launchPollState.pollHistory[launchPollState.pollHistoryCount++] = nowMs;
    return;
  }

  for (size_t i = 1; i < launchPollState.pollHistory.size(); ++i)
  {
    launchPollState.pollHistory[i - 1] = launchPollState.pollHistory[i];
  }
  launchPollState.pollHistory.back() = nowMs;
}

static void TickLaunchPolling()
{
  if (screenMode != LaunchScreenMode::Preview || !WiFi.isConnected())
  {
    return;
  }

  const unsigned long nowMs = millis();
  if (nowMs < launchPollState.nextPollAtMs)
  {
    return;
  }

  const LaunchSettings settings = activeSettings;
  const unsigned long pollBudgetPerHour = LaunchPollBudgetPerHour(settings);
  Serial.println(F("[LL2] polling next launch"));
  if (!HasLaunchPollBudget(nowMs, pollBudgetPerHour))
  {
    launchPollState.nextPollAtMs = NextLaunchPollBudgetTime(nowMs, pollBudgetPerHour);
    Serial.println(F("[LL2] budget exhausted; delaying poll"));
    return;
  }

  LaunchPreviewState updatedPreview = launchPollState.hasPreview ? launchPollState.preview : BuildPreviewState();
  String errorMessage;
  const bool refreshed = launchLibraryClient.RefreshNextLaunch(settings, updatedPreview, errorMessage);
  RecordLaunchPoll(nowMs);

  if (refreshed)
  {
    const bool changed =
        !launchPollState.hasPreview ||
        updatedPreview.rocket != launchPollState.preview.rocket ||
        updatedPreview.missionName != launchPollState.preview.missionName ||
        updatedPreview.launchSite != launchPollState.preview.launchSite ||
        updatedPreview.launchPad != launchPollState.preview.launchPad ||
        updatedPreview.scheduledDateTime != launchPollState.preview.scheduledDateTime ||
        updatedPreview.statusText != launchPollState.preview.statusText ||
        updatedPreview.liveDataReady != launchPollState.preview.liveDataReady ||
        updatedPreview.liveDataError != launchPollState.preview.liveDataError ||
        updatedPreview.rateLimited != launchPollState.preview.rateLimited ||
        updatedPreview.feedStatus != launchPollState.preview.feedStatus;

    launchPollState.preview = updatedPreview;
    launchPollState.hasPreview = true;
    launchPollState.lastError = String();
    screenMode = LaunchScreenMode::Preview;
    if (changed)
    {
      screenNeedsRedraw = true;
    }
    launchPollState.nextPollAtMs = nowMs + DesiredLaunchPollIntervalMs(settings, launchPollState.preview);
    Serial.print(F("[LL2] next poll in ms: "));
    Serial.println(launchPollState.nextPollAtMs - nowMs);
    return;
  }

  launchPollState.lastError = errorMessage;
  const bool isTransientFetchError = IsTransientLaunchFetchError(errorMessage);
  const bool isRateLimited = errorMessage.indexOf("HTTP 429") >= 0 || errorMessage.indexOf("rate limit") >= 0 || errorMessage.indexOf("Too Many Requests") >= 0;
  if (isRateLimited && launchPollState.hasPreview)
  {
    launchPollState.preview.rateLimited = true;
    launchPollState.preview.liveDataError = false;
    launchPollState.preview.feedStatus = "Rate limited; retrying in 1 hour.";
  }
  else if (isTransientFetchError && launchPollState.hasPreview)
  {
    launchPollState.preview.rateLimited = false;
    launchPollState.preview.liveDataError = false;
    launchPollState.preview.feedStatus = "Temporary network issue; retrying soon.";
  }
  else
  {
    launchPollState.preview = BuildPreviewState();
    launchPollState.preview.liveDataError = true;
    launchPollState.preview.rateLimited = false;
    launchPollState.preview.feedStatus = String("Live fetch failed: ") + errorMessage;
  }
  launchPollState.hasPreview = true;
  screenMode = LaunchScreenMode::Preview;
  screenNeedsRedraw = true;
  if (isRateLimited)
  {
    launchPollState.nextPollAtMs = nowMs + 60UL * 60UL * 1000UL;
  }
  else if (isTransientFetchError)
  {
    launchPollState.nextPollAtMs = nowMs + 2UL * 60UL * 1000UL;
  }
  else
  {
    launchPollState.nextPollAtMs = nowMs + 5UL * 60UL * 1000UL;
  }
  Serial.print(F("[LL2] poll failed: "));
  Serial.println(errorMessage);
}

static bool connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);

  WiFiManagerHelpers::ConfigureWiFiManager(wifiManager, renderPortalAssistScreen);

  if (!wifiManager.autoConnect(WiFiManagerHelpers::WiFiManagerName))
  {
    Serial.println("[WIFI] Failed to connect. Rebooting.");
    delay(2000);
    ESP.restart();
  }

  Serial.println("[WIFI] Connected.");
  Serial.print("[WIFI] IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(250);

  Serial.println();
  Serial.println("[BOOT] LaunchDisplay starting.");

  initDisplay();
  connectWiFi();
  RegisterLaunchSetupRoutes(launchSetupServer, wifiManager);
  launchSetupServer.begin();
  const LaunchSettings settings = LaunchSettingsStore::Load();

  if (settings.configured)
  {
    activeSettings = settings;
    LoadSleepSchedule(activeSettings);
    if (settings.launchScope == "within_distance" && (!settings.scopeResolved || !settings.scopeLocation.length()))
    {
      pendingScopeVerification = true;
      screenMode = LaunchScreenMode::Setup;
    }
    else
    {
      LaunchSettings resolvedSettings = settings;
      ResolveLaunchScopeIfNeeded(resolvedSettings);
      String timeSyncError;
      launchLibraryClient.SynchronizeClock(resolvedSettings.timeZone, timeSyncError);
      screenMode = LaunchScreenMode::Preview;
      launchPollState.hasPreview = false;
      launchPollState.nextPollAtMs = 0;
    }
  }
  else
  {
    screenMode = LaunchScreenMode::Setup;
  }
  screenNeedsRedraw = true;
  RedrawMainTft();
  screenNeedsRedraw = false;
  ApplySleepState();
  TickLaunchPolling();
}

void loop()
{
  launchSetupServer.handleClient();
  ResolvePendingLaunchScope();
  ApplySleepState();
  UpdateLiveCountdown();
  TickLaunchPolling();
  if (screenNeedsRedraw)
  {
    RedrawMainTft();
    screenNeedsRedraw = false;
  }
  delay(10);
}
