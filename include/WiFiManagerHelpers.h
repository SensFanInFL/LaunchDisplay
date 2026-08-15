#pragma once

#include <WiFiManager.h>
#include <vector>

namespace WiFiManagerHelpers
{
constexpr const char *WiFiManagerName = "LaunchDisplay-Setup";

template <typename PortalStartedCallback>
inline void ConfigureWiFiManager(WiFiManager &wm, PortalStartedCallback onPortalStarted)
{
  wm.setTitle("LaunchDisplay - Setup WiFi");
  wm.setConnectTimeout(10);
  wm.setConfigPortalTimeout(180);
  wm.setClass("invert");
  wm.setCustomHeadElement("<script>if(location.pathname==='/'||location.pathname==='')location.replace('/wifi');</script>");
  std::vector<const char *> menu = {"wifi"};
  wm.setMenu(menu);
  wm.setAPCallback([](WiFiManager *)
                   {
                     Serial.println("[WIFI] Configuration portal started.");
                     Serial.print("[WIFI] Join the AP named: ");
                     Serial.println(WiFiManagerName);
                   });
  wm.setWebServerCallback([onPortalStarted]()
                          {
    onPortalStarted(); });
}
} // namespace WiFiManagerHelpers
