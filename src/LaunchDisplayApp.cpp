#include <Arduino.h>

#include "DisplayConfig.h"
#include "LGFX.h"
#include "LaunchDisplayApp.h"

namespace
{
constexpr int kMargin = 18;
constexpr int kCardInset = 24;

void DrawCenteredText(LGFX &tft, const String &text, int y, uint16_t color, const lgfx::IFont *font, textdatum_t datum = middle_center)
{
  tft.setTextDatum(datum);
  tft.setFont(font);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, TFT_WIDTH / 2, y);
}

void DrawCard(LGFX &tft, int y, const char *label, const String &value, uint16_t accentColor)
{
  const int width = TFT_WIDTH - (kMargin * 2);
  const int x = kMargin;
  const int height = 68;

  tft.drawRoundRect(x, y, width, height, 10, accentColor);
  tft.fillRoundRect(x + 1, y + 1, width - 2, height - 2, TFT_BLACK);

  tft.setTextDatum(top_left);
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(accentColor, TFT_BLACK);
  tft.drawString(label, x + 14, y + 10);

  tft.setTextDatum(middle_left);
  tft.setFont(&fonts::FreeSans12pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(value, x + 14, y + (height / 2) + 8);
}

void DrawDivider(LGFX &tft, int y)
{
  tft.drawFastHLine(28, y, TFT_WIDTH - 56, TFT_DARKGREY);
}

void DrawWrappedText(LGFX &tft, const String &text, int x, int y, int maxWidth, uint16_t color, const lgfx::IFont *font, int lineHeight)
{
  tft.setFont(font);
  tft.setTextDatum(top_left);
  tft.setTextColor(color, TFT_BLACK);

  String line;
  int currentY = y;
  int wordStart = 0;
  while (wordStart < static_cast<int>(text.length()))
  {
    int wordEnd = text.indexOf(' ', wordStart);
    if (wordEnd < 0)
    {
      wordEnd = text.length();
    }

    String word = text.substring(wordStart, wordEnd);
    String candidate = line.length() ? line + " " + word : word;
    if (line.length() && tft.textWidth(candidate) > maxWidth)
    {
      tft.drawString(line, x, currentY);
      currentY += lineHeight;
      line = word;
    }
    else
    {
      line = candidate;
    }

    wordStart = wordEnd + 1;
  }

  if (line.length())
  {
    tft.drawString(line, x, currentY);
  }
}

} // namespace

void RenderLaunchSplashScreen(LGFX &tft, const String &subtitle)
{
  tft.fillScreen(TFT_BLACK);

  DrawCenteredText(tft, "LAUNCHDISPLAY", 164, TFT_GREEN, &fonts::FreeSans18pt7b);
  DrawCenteredText(tft, subtitle, 214, TFT_WHITE, &fonts::FreeSans12pt7b);
}

void RenderLaunchWiFiSetupScreen(LGFX &tft, const String &ssidName)
{
  tft.fillScreen(TFT_BLACK);

  DrawCenteredText(tft, "LaunchDisplay Wi-Fi Setup", 84, TFT_GREEN, &fonts::FreeSans12pt7b);
  DrawCenteredText(tft, "Connect to the SSID below", 122, TFT_LIGHTGREY, &fonts::FreeSans9pt7b);
  DrawCenteredText(tft, "to connect this device to Wi-Fi.", 144, TFT_LIGHTGREY, &fonts::FreeSans9pt7b);

  tft.setTextDatum(top_left);
  tft.setFont(&fonts::FreeSans12pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(ssidName, 58, 228);

  DrawCenteredText(tft, "After Wi-Fi connects,", 344, TFT_LIGHTGREY, &fonts::FreeSans9pt7b);
  DrawCenteredText(tft, "setup instructions", 364, TFT_LIGHTGREY, &fonts::FreeSans9pt7b);
  DrawCenteredText(tft, "will be displayed.", 384, TFT_LIGHTGREY, &fonts::FreeSans9pt7b);
}

void RenderLaunchStandbyScreen(LGFX &tft, const LaunchDisplayStatus &status)
{
  tft.fillScreen(TFT_BLACK);

  DrawCenteredText(tft, "STANDBY", 84, TFT_GREEN, &fonts::FreeSans18pt7b);
  DrawCenteredText(tft, "Awaiting launch data", 122, TFT_LIGHTGREY, &fonts::FreeSans12pt7b);

  const int firstCardY = 172;
  DrawCard(tft, firstCardY, "Wi-Fi", status.wifiConnected ? "Connected" : "Setup mode", TFT_GREEN);
  DrawCard(tft, firstCardY + 80, "IP", status.wifiIp, TFT_CYAN);
  DrawCard(tft, firstCardY + 160, "Feed", "Not connected yet", TFT_ORANGE);
}

void RenderLaunchWaitingScreen(LGFX &tft, const String &message)
{
  tft.fillScreen(TFT_BLACK);
  DrawCenteredText(tft, message.length() ? message : String("Waiting for launch data"), 240, TFT_WHITE, &fonts::FreeSans12pt7b);
}

void RenderLaunchPreviewScreen(LGFX &tft, const LaunchPreviewState &state)
{
  if (state.liveDataError)
  {
    tft.fillScreen(TFT_BLACK);
    DrawCenteredText(tft, "LIVE FETCH", 38, TFT_ORANGE, &fonts::FreeSans18pt7b);
    DrawCenteredText(tft, "FAILED", 64, TFT_ORANGE, &fonts::FreeSans18pt7b);
    DrawCenteredText(tft, "No live launch data was loaded.", 88, TFT_LIGHTGREY, &fonts::FreeSans9pt7b);
    DrawWrappedText(tft, state.feedStatus, 18, 118, TFT_WIDTH - 36, TFT_WHITE, &fonts::FreeSans9pt7b, 18);
    DrawWrappedText(tft, "The device will retry automatically after a short backoff.", 18, 240, TFT_WIDTH - 36, TFT_LIGHTGREY, &fonts::FreeSans9pt7b, 18);
    DrawWrappedText(tft, "Check Wi-Fi, the API key, and the serial log if this keeps happening.", 18, 284, TFT_WIDTH - 36, TFT_LIGHTGREY, &fonts::FreeSans9pt7b, 18);
    return;
  }

  tft.fillScreen(TFT_BLACK);

  DrawCenteredText(tft, state.rocket, 48, TFT_GREEN, &fonts::FreeSans18pt7b);
  DrawCenteredText(tft, state.missionName, 104, TFT_WHITE, &fonts::FreeSans12pt7b);

  DrawCenteredText(tft, state.launchSite, 166, TFT_LIGHTGREY, &fonts::FreeSans9pt7b);
  DrawCenteredText(tft, state.launchPad, 194, TFT_LIGHTGREY, &fonts::FreeSans12pt7b);

  DrawCenteredText(tft, state.scheduledDateTime, 250, TFT_WHITE, &fonts::FreeSans9pt7b);

  DrawCenteredText(tft, state.countdown, 350, TFT_GREEN, &fonts::FreeMono18pt7b);

  const uint16_t statusColor =
      state.statusText == "GO" ? TFT_GREEN :
      state.statusText == "HOLD" ? TFT_ORANGE :
      state.statusText == "DELAYED" ? TFT_ORANGE :
      state.statusText == "SCRUBBED" ? TFT_RED :
      state.statusText == "LAUNCHED" ? TFT_CYAN :
      state.statusText == "RUD" ? TFT_RED :
      TFT_WHITE;

  DrawCenteredText(tft, state.statusText, 426, statusColor, &fonts::FreeSans18pt7b);

  const uint16_t feedColor = state.rateLimited ? TFT_ORANGE : (state.liveDataReady ? TFT_GREEN : TFT_ORANGE);
  DrawCenteredText(tft, state.feedStatus, 462, feedColor, &fonts::FreeSans9pt7b);
}

void UpdateLaunchPreviewCountdown(LGFX &tft, const String &countdown)
{
  const int updateTop = 312;
  const int updateHeight = 64;

  tft.fillRect(0, updateTop, TFT_WIDTH, updateHeight, TFT_BLACK);
  DrawCenteredText(tft, countdown, 350, TFT_GREEN, &fonts::FreeMono18pt7b);
}

void RenderLaunchSetupScreen(LGFX &tft, const LaunchSetupState &state)
{
  tft.fillScreen(TFT_BLACK);

  DrawCenteredText(tft, "LaunchDisplay Setup", 18, TFT_GREEN, &fonts::FreeSans12pt7b);
  DrawCenteredText(tft, "Use the portal for API key", 40, TFT_LIGHTGREY, &fonts::FreeSans9pt7b);
  DrawCenteredText(tft, "and launch filters", 56, TFT_LIGHTGREY, &fonts::FreeSans9pt7b);

  const int qrSize = 118;
  const int qrX = 101;
  const int qrY = 80;
  tft.drawRoundRect(qrX - 6, qrY - 6, qrSize + 12, qrSize + 12, 8, TFT_WHITE);
  tft.fillRoundRect(qrX - 5, qrY - 5, qrSize + 10, qrSize + 10, TFT_BLACK);
  tft.qrcode(state.portalUrl.c_str(), qrX, qrY, qrSize, 3);

  tft.setTextDatum(top_left);
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("SSID", 16, 212);
  tft.setFont(&fonts::FreeSans12pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(state.wifiSsid, 16, 232);

  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Portal IP", 16, 262);
  tft.setFont(&fonts::FreeSans12pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(state.deviceIp, 16, 282);

  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(state.setupHint, 16, 334);
  tft.drawString(state.footerLine1, 16, 354);
}
