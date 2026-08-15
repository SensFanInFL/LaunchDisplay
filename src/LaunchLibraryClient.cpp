#include "LaunchLibraryClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <time.h>

namespace
{
constexpr unsigned long kClockSyncTimeoutMs = 10000;
constexpr time_t kClockValidThreshold = 1700000000; // 2023-11-ish

time_t TimegmPortable(struct tm utc)
{
  const int year = utc.tm_year + 1900;
  const int month = utc.tm_mon + 1;
  const int day = utc.tm_mday;

  const int y = month <= 2 ? year - 1 : year;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

  const long long days = static_cast<long long>(era) * 146097LL + static_cast<long long>(doe) - 719468LL;
  return static_cast<time_t>(days * 86400LL + utc.tm_hour * 3600LL + utc.tm_min * 60LL + utc.tm_sec);
}

String UrlEncode(const String &input)
{
  String output;
  output.reserve(input.length() * 3);
  for (size_t i = 0; i < input.length(); ++i)
  {
    const char c = input[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
    {
      output += c;
    }
    else if (c == ' ')
    {
      output += F("%20");
    }
    else
    {
      char encoded[4];
      snprintf(encoded, sizeof(encoded), "%%%02X", static_cast<unsigned char>(c));
      output += encoded;
    }
  }
  return output;
}

String PosixTimeZoneForIana(const String &timeZone)
{
  if (timeZone == "America/New_York")
  {
    return "EST5EDT,M3.2.0,M11.1.0";
  }
  if (timeZone == "America/Chicago")
  {
    return "CST6CDT,M3.2.0,M11.1.0";
  }
  if (timeZone == "America/Denver")
  {
    return "MST7MDT,M3.2.0,M11.1.0";
  }
  if (timeZone == "America/Los_Angeles")
  {
    return "PST8PDT,M3.2.0,M11.1.0";
  }
  if (timeZone == "America/Phoenix")
  {
    return "MST7";
  }
  if (timeZone == "Europe/London")
  {
    return "GMT0BST,M3.5.0/1,M10.5.0";
  }
  if (timeZone == "Europe/Paris")
  {
    return "CET-1CEST,M3.5.0,M10.5.0/3";
  }
  if (timeZone == "Asia/Tokyo")
  {
    return "JST-9";
  }
  if (timeZone == "Australia/Sydney")
  {
    return "AEST-10AEDT,M10.1.0,M4.1.0/3";
  }
  return "UTC0";
}

String BuildLaunchUrl(const LaunchSettings &settings)
{
  if (settings.launchScope == "worldwide")
  {
    return F("https://ll.thespacedevs.com/2.3.0/launches/upcoming/?limit=1&ordering=net&hide_recent_previous=true");
  }

  return F("https://ll.thespacedevs.com/2.3.0/launches/upcoming/?limit=25&ordering=net&hide_recent_previous=true");
}

String BuildLocationUrl(const String &query)
{
  String url = F("https://ll.thespacedevs.com/2.3.0/locations/?limit=10&search=");
  url += UrlEncode(query);
  return url;
}

String BuildNominatimUrl(const String &query)
{
  String url = F("https://nominatim.openstreetmap.org/search?format=jsonv2&addressdetails=1&limit=5&q=");
  url += UrlEncode(query);
  return url;
}

String ReadJsonString(const JsonVariantConst &value)
{
  if (value.is<const char *>())
  {
    return String(value.as<const char *>());
  }
  return String();
}

String NormalizeLocationQuery(const String &input)
{
  String value = input;
  value.toLowerCase();
  value.replace(".", "");
  value.replace(",", "");
  value.replace(" ", "");
  value.replace("-", "");
  value.replace("_", "");
  return value;
}

String FirstLocationTerm(const String &query)
{
  const int comma = query.indexOf(',');
  if (comma <= 0)
  {
    return query;
  }

  String firstTerm = query.substring(0, comma);
  firstTerm.trim();
  return firstTerm.length() ? firstTerm : query;
}

int ScoreLocationMatch(const String &query, const String &candidateName)
{
  const String normalizedQuery = NormalizeLocationQuery(query);
  const String normalizedStem = NormalizeLocationQuery(FirstLocationTerm(query));
  const String normalizedCandidate = NormalizeLocationQuery(candidateName);

  int score = 0;
  if (normalizedCandidate == normalizedQuery)
  {
    score += 120;
  }
  if (normalizedCandidate == normalizedStem)
  {
    score += 100;
  }
  if (normalizedStem.length() && normalizedCandidate.startsWith(normalizedStem))
  {
    score += 60;
  }
  if (normalizedQuery.length() && normalizedCandidate.indexOf(normalizedQuery) >= 0)
  {
    score += 40;
  }
  if (normalizedStem.length() && normalizedCandidate.indexOf(normalizedStem) >= 0)
  {
    score += 30;
  }
  return score;
}

bool ResolveLocationViaNominatim(const String &query, float &latitude, float &longitude, String &resolvedName, String &errorMessage)
{
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30000);

  HTTPClient http;
  const String url = BuildNominatimUrl(query);
  Serial.print(F("[GEOCODE] GET "));
  Serial.println(url);
  http.useHTTP10(true);
  if (!http.begin(client, url))
  {
    errorMessage = F("Unable to start geocoding request.");
    return false;
  }

  http.setConnectTimeout(15000);
  http.setTimeout(20000);
  http.addHeader("Accept", "application/json");
  http.addHeader("User-Agent", "LaunchDisplay/1.0");
  const int statusCode = http.GET();
  Serial.print(F("[GEOCODE] HTTP status: "));
  Serial.println(statusCode);
  if (statusCode != HTTP_CODE_OK)
  {
    errorMessage = String(F("Geocoding returned HTTP ")) + statusCode;
    http.end();
    return false;
  }

  DynamicJsonDocument doc(6 * 1024);
  const DeserializationError parseError = deserializeJson(doc, http.getStream(), DeserializationOption::NestingLimit(12));
  http.end();
  if (parseError)
  {
    errorMessage = String(F("Geocoding JSON parse failed: ")) + parseError.c_str();
    return false;
  }

  JsonArrayConst results = doc.as<JsonArrayConst>();
  if (results.isNull() || results.size() == 0)
  {
    errorMessage = F("No geocoding match found.");
    return false;
  }

  for (JsonObjectConst location : results)
  {
    const String latText = ReadJsonString(location["lat"]);
    const String lonText = ReadJsonString(location["lon"]);
    if (!latText.length() || !lonText.length())
    {
      continue;
    }

    latitude = latText.toFloat();
    longitude = lonText.toFloat();
    resolvedName = ReadJsonString(location["display_name"]);
    if (!resolvedName.length())
    {
      resolvedName = query;
    }
    return true;
  }

  errorMessage = F("Geocoding coordinates missing.");
  return false;
}

StaticJsonDocument<1024> BuildLaunchFilter()
{
  StaticJsonDocument<1024> filter;
  JsonObject result = filter["results"][0].to<JsonObject>();
  result["rocket"]["configuration"]["full_name"] = true;
  result["rocket"]["configuration"]["name"] = true;
  result["mission"]["name"] = true;
  result["pad"]["location"]["name"] = true;
  result["pad"]["location"]["country"]["name"] = true;
  result["pad"]["latitude"] = true;
  result["pad"]["longitude"] = true;
  result["pad"]["name"] = true;
  result["net"] = true;
  result["status"]["name"] = true;
  result["status"]["abbrev"] = true;
  return filter;
}

StaticJsonDocument<256> BuildLocationFilter()
{
  StaticJsonDocument<256> filter;
  JsonObject result = filter["results"][0].to<JsonObject>();
  result["latitude"] = true;
  result["longitude"] = true;
  result["name"] = true;
  return filter;
}

String NormalizeStatus(const String &rawStatus)
{
  String status = rawStatus;
  status.toLowerCase();
  if (status.indexOf("scrub") >= 0)
  {
    return "SCRUBBED";
  }
  if (status.indexOf("hold") >= 0)
  {
    return "HOLD";
  }
  if (status.indexOf("delay") >= 0)
  {
    return "DELAYED";
  }
  if (status.indexOf("launch successful") >= 0 || status.indexOf("success") >= 0 || status.indexOf("launched") >= 0)
  {
    return "LAUNCHED";
  }
  if (status.indexOf("rud") >= 0 || status.indexOf("failure") >= 0)
  {
    return "RUD";
  }
  if (status.indexOf("go") >= 0)
  {
    return "GO";
  }
  return "GO";
}

bool ParseIsoUtc(const String &iso, time_t &epochUtc)
{
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
  {
    return false;
  }

  struct tm utc = {};
  utc.tm_year = year - 1900;
  utc.tm_mon = month - 1;
  utc.tm_mday = day;
  utc.tm_hour = hour;
  utc.tm_min = minute;
  utc.tm_sec = second;
  utc.tm_isdst = 0;

  epochUtc = TimegmPortable(utc);
  return epochUtc > 0;
}

String FormatLocalDateTime(time_t epochUtc)
{
  struct tm local = {};
  localtime_r(&epochUtc, &local);

  char buffer[48];
  if (strftime(buffer, sizeof(buffer), "%B %d @ %I:%M %p", &local) == 0)
  {
    return String();
  }

  String output = buffer;
  if (output.startsWith("0"))
  {
    output.remove(0, 1);
  }
  output.replace(" 0", " ");
  return output;
}

String FormatLocalDateTimeInZone(time_t epochUtc, const String &timeZone)
{
  if (timeZone.length())
  {
    const String posixTimeZone = PosixTimeZoneForIana(timeZone);
    setenv("TZ", posixTimeZone.c_str(), 1);
    tzset();
  }

  return FormatLocalDateTime(epochUtc);
}

String FormatLocalTime(time_t epochUtc)
{
  struct tm local = {};
  localtime_r(&epochUtc, &local);

  char buffer[24];
  if (strftime(buffer, sizeof(buffer), "%I:%M %p", &local) == 0)
  {
    return String();
  }

  String output = buffer;
  if (output.startsWith("0"))
  {
    output.remove(0, 1);
  }
  return output;
}

String FormatLocalTimeInZone(time_t epochUtc, const String &timeZone)
{
  if (timeZone.length())
  {
    const String posixTimeZone = PosixTimeZoneForIana(timeZone);
    setenv("TZ", posixTimeZone.c_str(), 1);
    tzset();
  }

  return FormatLocalTime(epochUtc);
}

String FormatCountdown(time_t launchEpochUtc)
{
  const time_t now = time(nullptr);
  if (now < kClockValidThreshold)
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

String ExtractString(const JsonVariantConst &value)
{
  if (value.is<const char *>())
  {
    return String(value.as<const char *>());
  }
  return String();
}

bool ExtractFloat(const JsonVariantConst &value, float &out)
{
  if (value.isNull())
  {
    return false;
  }

  out = value.as<float>();
  return true;
}

void LogBodySnippet(const String &body)
{
  const int snippetLength = body.length() > 200 ? 200 : body.length();
  Serial.print(F("[LL2] body snippet: "));
  Serial.println(body.substring(0, snippetLength));
}

double DegreesToRadians(double degrees)
{
  return degrees * 0.017453292519943295;
}

double HaversineDistance(float latitudeA, float longitudeA, float latitudeB, float longitudeB, bool miles)
{
  const double earthRadius = miles ? 3958.7613 : 6371.0088;
  const double dLat = DegreesToRadians(latitudeB - latitudeA);
  const double dLon = DegreesToRadians(longitudeB - longitudeA);
  const double lat1 = DegreesToRadians(latitudeA);
  const double lat2 = DegreesToRadians(latitudeB);
  const double sinHalfLat = sin(dLat / 2.0);
  const double sinHalfLon = sin(dLon / 2.0);
  const double a = sinHalfLat * sinHalfLat + cos(lat1) * cos(lat2) * sinHalfLon * sinHalfLon;
  const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return earthRadius * c;
}

String NormalizedCountry(const String &country)
{
  String value = country;
  value.toLowerCase();
  value.replace(".", "");
  value.replace(",", "");
  value.replace(" ", "");

  if (value == "unitedstatesofamerica" || value == "usa" || value == "us" || value == "unitedstates")
  {
    return "unitedstates";
  }

  if (value == "unitedkingdom" || value == "uk" || value == "greatbritain")
  {
    return "unitedkingdom";
  }

  return value;
}

bool ResultMatchesScope(const JsonObjectConst &launch, const LaunchSettings &settings, String &rejectReason)
{
  rejectReason = "";
  if (settings.launchScope == "worldwide")
  {
    return true;
  }

  const String locationCountry = ExtractString(launch["pad"]["location"]["country"]["name"]);
  const String locationName = ExtractString(launch["pad"]["location"]["name"]);

  if (settings.launchScope == "country")
  {
    if (NormalizedCountry(locationCountry) == NormalizedCountry(settings.country))
    {
      return true;
    }
    rejectReason = locationCountry;
    return false;
  }

  if (settings.launchScope == "within_distance")
  {
    if (!settings.scopeResolved || settings.scopeLatitude == 0.0f || settings.scopeLongitude == 0.0f)
    {
      rejectReason = "scope not resolved";
      return false;
    }

    float launchLat = 0.0f;
    float launchLon = 0.0f;
    if (!ExtractFloat(launch["pad"]["latitude"], launchLat) || !ExtractFloat(launch["pad"]["longitude"], launchLon))
    {
      rejectReason = "launch coordinates unavailable";
      return false;
    }

    const bool useMiles = settings.scopeUnits != "kilometers";
    const double radius = settings.scopeRadius.length() ? settings.scopeRadius.toFloat() : 0.0f;
    const double distance = HaversineDistance(settings.scopeLatitude, settings.scopeLongitude, launchLat, launchLon, useMiles);
    if (distance <= radius)
    {
      return true;
    }

    rejectReason = locationName;
    return false;
  }

  return true;
}
} // namespace

bool LaunchLibraryClient::ResolveScopeLocation(const String &query, float &latitude, float &longitude, String &resolvedName, String &errorMessage)
{
  String trimmedQuery = query;
  trimmedQuery.trim();
  if (!trimmedQuery.length())
  {
    errorMessage = F("Location is empty.");
    return false;
  }

  String geocodeError;
  if (ResolveLocationViaNominatim(trimmedQuery, latitude, longitude, resolvedName, geocodeError))
  {
    Serial.print(F("[GEOCODE] resolved location: "));
    Serial.print(resolvedName);
    Serial.print(F(" @ "));
    Serial.print(latitude, 4);
    Serial.print(F(", "));
    Serial.println(longitude, 4);
    return true;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30000);

  HTTPClient http;
  const String url = BuildLocationUrl(trimmedQuery);
  Serial.print(F("[LL2] RESOLVE "));
  Serial.println(url);
  http.useHTTP10(true);
  if (!http.begin(client, url))
  {
    errorMessage = F("Unable to start location lookup.");
    return false;
  }

  http.setConnectTimeout(15000);
  http.setTimeout(20000);
  http.addHeader("Accept", "application/json");
  const int statusCode = http.GET();
  Serial.print(F("[LL2] location HTTP status: "));
  Serial.println(statusCode);
  if (statusCode != HTTP_CODE_OK)
  {
    errorMessage = String(F("Location lookup returned HTTP ")) + statusCode;
    http.end();
    return false;
  }

  DynamicJsonDocument doc(6 * 1024);
  const DeserializationError parseError = deserializeJson(doc, http.getStream(), DeserializationOption::NestingLimit(12));
  http.end();
  if (parseError)
  {
    errorMessage = String(F("Location lookup JSON parse failed: ")) + parseError.c_str();
    return false;
  }

  JsonArrayConst results = doc["results"].as<JsonArrayConst>();
  if (results.isNull() || results.size() == 0)
  {
    const String shorterQuery = FirstLocationTerm(trimmedQuery);
    if (shorterQuery.length() && shorterQuery != trimmedQuery)
    {
      return ResolveScopeLocation(shorterQuery, latitude, longitude, resolvedName, errorMessage);
    }

    errorMessage = F("No matching location found.");
    return false;
  }

  int bestScore = -1;
  float foundLatitude = 0.0f;
  float foundLongitude = 0.0f;
  String foundName;

  for (size_t i = 0; i < results.size(); ++i)
  {
    JsonObjectConst location = results[i].as<JsonObjectConst>();
    if (location.isNull())
    {
      continue;
    }

    float candidateLatitude = 0.0f;
    float candidateLongitude = 0.0f;
    if (!ExtractFloat(location["latitude"], candidateLatitude) || !ExtractFloat(location["longitude"], candidateLongitude))
    {
      continue;
    }

    String candidateName = ExtractString(location["name"]);
    const int score = ScoreLocationMatch(trimmedQuery, candidateName);
    if (score > bestScore)
    {
      bestScore = score;
      foundLatitude = candidateLatitude;
      foundLongitude = candidateLongitude;
      foundName = candidateName.length() ? candidateName : trimmedQuery;
    }
  }

  if (bestScore < 0)
  {
    errorMessage = F("No matching location found.");
    return false;
  }

  latitude = foundLatitude;
  longitude = foundLongitude;
  resolvedName = foundName;
  if (!resolvedName.length())
  {
    resolvedName = trimmedQuery;
  }

  Serial.print(F("[LL2] resolved location: "));
  Serial.print(resolvedName);
  Serial.print(F(" @ "));
  Serial.print(latitude, 4);
  Serial.print(F(", "));
  Serial.println(longitude, 4);
  return true;
}

bool LaunchLibraryClient::SynchronizeClock(const String &timeZone, String &errorMessage)
{
  const String posixTimeZone = PosixTimeZoneForIana(timeZone);
  configTzTime(posixTimeZone.c_str(), "pool.ntp.org", "time.nist.gov", "time.google.com");

  const unsigned long start = millis();
  while ((millis() - start) < kClockSyncTimeoutMs)
  {
    time_t now = time(nullptr);
    if (now > kClockValidThreshold)
    {
      return true;
    }
    delay(250);
  }

  errorMessage = F("NTP sync timed out.");
  return false;
}

bool LaunchLibraryClient::RefreshNextLaunch(const LaunchSettings &settings, LaunchPreviewState &preview, String &errorMessage)
{
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30000);

  HTTPClient http;
  const String url = BuildLaunchUrl(settings);
  Serial.print(F("[LL2] GET "));
  Serial.println(url);
  http.useHTTP10(true);
  if (!http.begin(client, url))
  {
    errorMessage = F("Unable to start LL2 request.");
    Serial.println(F("[LL2] begin failed"));
    return false;
  }

  http.setConnectTimeout(15000);
  http.setTimeout(20000);
  http.addHeader("Accept", "application/json");
  if (settings.apiKey.length())
  {
    http.addHeader("X-API-Key", settings.apiKey);
    http.addHeader("Authorization", String("Bearer ") + settings.apiKey);
  }
  const int statusCode = http.GET();
  Serial.print(F("[LL2] HTTP status: "));
  Serial.println(statusCode);
  if (statusCode != HTTP_CODE_OK)
  {
    errorMessage = String(F("LL2 returned HTTP ")) + statusCode;
    http.end();
    return false;
  }

  DynamicJsonDocument doc(16 * 1024);
  const DeserializationError parseError = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(BuildLaunchFilter()), DeserializationOption::NestingLimit(20));
  http.end();
  if (parseError)
  {
    errorMessage = String(F("LL2 JSON parse failed: ")) + parseError.c_str();
    Serial.print(F("[LL2] parse failed: "));
    Serial.println(parseError.c_str());
    return false;
  }

  JsonArrayConst results = doc["results"].as<JsonArrayConst>();
  if (results.isNull() || results.size() == 0)
  {
    errorMessage = F("LL2 returned no upcoming launch.");
    Serial.println(F("[LL2] no upcoming launch in results[0]"));
    return false;
  }

  JsonObjectConst launch;
  String rejectedReason;
  for (JsonObjectConst candidate : results)
  {
    if (ResultMatchesScope(candidate, settings, rejectedReason))
    {
      launch = candidate;
      break;
    }
  }

  if (launch.isNull())
  {
    errorMessage = String(F("No launch matched scope: ")) + rejectedReason;
    Serial.print(F("[LL2] "));
    Serial.println(errorMessage);
    return false;
  }

  const String rocket = ExtractString(launch["rocket"]["configuration"]["full_name"]);
  const String fallbackRocket = ExtractString(launch["rocket"]["configuration"]["name"]);
  const String mission = ExtractString(launch["mission"]["name"]);
  const String site = ExtractString(launch["pad"]["location"]["name"]);
  const String pad = ExtractString(launch["pad"]["name"]);
  const String net = ExtractString(launch["net"]);
  const String statusRaw = ExtractString(launch["status"]["name"]);
  const String statusAbbrev = ExtractString(launch["status"]["abbrev"]);

  time_t launchEpochUtc = 0;
  String scheduledDateTime = net;
  String countdown = "T-??:??:??";
  if (ParseIsoUtc(net, launchEpochUtc))
  {
    const time_t now = time(nullptr);
    if (now > kClockValidThreshold)
    {
      scheduledDateTime = FormatLocalDateTimeInZone(launchEpochUtc, settings.timeZone);
      countdown = FormatCountdown(launchEpochUtc);
    }
    else
    {
      scheduledDateTime = net;
    }
  }

  preview.rocket = rocket.length() ? rocket : fallbackRocket;
  preview.missionName = mission.length() ? mission : "Mission details unavailable";
  if (site.length() && pad.length())
  {
    preview.launchSite = site;
    preview.launchPad = pad;
  }
  else
  {
    preview.launchSite = site.length() ? site : "Launch site unavailable";
    preview.launchPad = pad.length() ? pad : "Pad unavailable";
  }
  preview.scheduledDateTime = scheduledDateTime;
  preview.scheduledEpochUtc = launchEpochUtc;
  preview.countdown = countdown;
  preview.statusText = NormalizeStatus(statusRaw.length() ? statusRaw : statusAbbrev);
  preview.status.wifiConnected = true;
  preview.status.wifiIp = WiFi.localIP().toString();
  preview.status.wifiSsid = WiFi.SSID();
  preview.liveDataReady = true;
  preview.liveDataError = false;
  const time_t refreshedAt = time(nullptr);
  const String refreshedTime = FormatLocalTimeInZone(refreshedAt, settings.timeZone);
  preview.feedStatus = refreshedTime.length() ? String("Last updated ") + refreshedTime : String("Last updated");

  errorMessage = String();
  Serial.println(F("[LL2] refresh ok"));
  return true;
}
