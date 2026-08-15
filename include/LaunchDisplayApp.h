#pragma once

#include <Arduino.h>
#include <time.h>

class LGFX;

struct LaunchDisplayStatus
{
  bool wifiConnected;
  String wifiIp;
  String wifiSsid;
};

struct LaunchPreviewState
{
  LaunchDisplayStatus status;
  String rocket;
  String missionName;
  String launchSite;
  String launchPad;
  String scheduledDateTime;
  time_t scheduledEpochUtc;
  String countdown;
  String statusText;
  bool liveDataReady;
  bool liveDataError;
  bool rateLimited;
  String feedStatus;
};

struct LaunchSetupState
{
  String wifiSsid;
  String portalUrl;
  String deviceIp;
  String setupHint;
  String footerLine1;
  String footerLine2;
};

void RenderLaunchSplashScreen(LGFX &tft, const String &subtitle);
void RenderLaunchWiFiSetupScreen(LGFX &tft, const String &ssidName);
void RenderLaunchStandbyScreen(LGFX &tft, const LaunchDisplayStatus &status);
void RenderLaunchWaitingScreen(LGFX &tft, const String &message);
void RenderLaunchPreviewScreen(LGFX &tft, const LaunchPreviewState &state);
void UpdateLaunchPreviewCountdown(LGFX &tft, const String &countdown);
void RenderLaunchSetupScreen(LGFX &tft, const LaunchSetupState &state);
