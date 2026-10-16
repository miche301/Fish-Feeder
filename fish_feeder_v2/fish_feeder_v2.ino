#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <time.h>
#include <math.h>

// ---------- Change these ----------
const char* WIFI_SSID     = "wifi";      // must be a 2.4 GHz network
const char* WIFI_PASSWORD = "password";
const unsigned long FEED_INTERVAL_HOURS = 24;      // time between feedings
const bool TEST_MODE = false;                      // true = 1-minute interval for testing
// ----------------------------------

const int BUTTON_PIN = 4;
const int LED_PIN    = 2;
const int TFT_CS     = 5;
const int TFT_DC     = 16;
const int TFT_RST    = 17;
const char* TIMEZONE = "PST8PDT,M3.2.0,M11.1.0";   // Pacific time with daylight saving
const char* HOSTNAME = "fishfeeder";               // page at http://fishfeeder.local
const int HISTORY_SIZE = 5;

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);    // uses hardware SPI (D18, D23)
WebServer server(80);
Preferences prefs;

int64_t history[HISTORY_SIZE] = {0};   // newest first; 0 = empty slot

// Button debounce state
int lastReading = HIGH;
int stableState = HIGH;
unsigned long lastChangeMs = 0;
const unsigned long DEBOUNCE_MS = 50;

// LED / icon blink state
unsigned long lastBlinkMs = 0;
bool blinkOn = false;

// ---------- Screen layout & colors ----------

const int CX = 120, CY = 120;          // screen center
const int RING_OUTER = 118;
const int RING_INNER = 104;

uint16_t COLOR_BG, COLOR_TEXT, COLOR_DIM, COLOR_TRACK, COLOR_GREEN, COLOR_ORANGE;

// What's currently drawn, so we only redraw what changed (avoids flicker)
struct Shown {
  String text = "\x01";   // impossible value forces the first draw
  uint16_t color = 0;
};
Shown shownBig, shownLabel, shownLast;
int shownRingPct = -1;
uint16_t shownRingColor = 0;
int shownFishColor = -1;

// ---------- Time helpers ----------

time_t lastFed() { return (time_t)history[0]; }

unsigned long intervalSec() {
  return TEST_MODE ? 60UL : FEED_INTERVAL_HOURS * 3600UL;
}

bool timeIsSynced() {
  // Before the internet time sync finishes, the clock reads 1970
  return time(nullptr) > 1700000000;
}

bool needsFeeding() {
  if (lastFed() == 0) return true;
  if (!timeIsSynced()) return false;
  return (time(nullptr) - lastFed()) >= (time_t)intervalSec();
}

String formatTime(time_t t, const char* fmt) {
  struct tm info;
  localtime_r(&t, &info);
  char buf[40];
  strftime(buf, sizeof(buf), fmt, &info);
  String s(buf);
  if (s.startsWith("0")) s.remove(0, 1);   // "07:30 PM" -> "7:30 PM"
  return s;
}

String formatAgo(long seconds) {
  if (seconds < 60) return "just now";
  long minutes = seconds / 60;
  long hours = minutes / 60;
  long days = hours / 24;
  minutes %= 60;
  hours %= 24;

  String s = "";
  if (days > 0)  s += String(days) + " d ";
  if (hours > 0) s += String(hours) + " h ";
  if (days == 0) s += String(minutes) + " min ";
  return s + "ago";
}

// Short countdown for the screen: "23h 05m", "42m", or "37s"
String formatCountdown(long sec) {
  char buf[16];
  if (sec >= 3600) snprintf(buf, sizeof(buf), "%ldh %02ldm", sec / 3600, (sec % 3600) / 60);
  else if (sec >= 120) snprintf(buf, sizeof(buf), "%ldm", sec / 60);
  else snprintf(buf, sizeof(buf), "%lds", sec);
  return String(buf);
}

// ---------- Storage ----------

void saveHistory() {
  prefs.putBytes("history", history, sizeof(history));
}

void loadHistory() {
  size_t n = prefs.getBytes("history", history, sizeof(history));
  if (n != sizeof(history)) {
    memset(history, 0, sizeof(history));
    // Carry over the single saved time from v1, if there is one
    history[0] = prefs.getLong64("lastFed", 0);
  }
}

void recordFeeding() {
  if (!timeIsSynced()) {
    Serial.println("Clock isn't synced yet. Check Wi-Fi and try again in a few seconds.");
    return;
  }
  for (int i = HISTORY_SIZE - 1; i > 0; i--) history[i] = history[i - 1];
  history[0] = (int64_t)time(nullptr);
  saveHistory();
  Serial.println("Fed at " + formatTime(lastFed(), "%a %b %d, %I:%M %p"));

  // Quick confirmation flash
  digitalWrite(LED_PIN, HIGH);
  delay(300);
  digitalWrite(LED_PIN, LOW);
  blinkOn = false;
  updateScreen();   // show the new countdown right away
}

// ---------- Drawing helpers ----------

// Thick arc between two angles (degrees, 0 = top, clockwise)
void drawArc(float a0, float a1, uint16_t color) {
  const float STEP = 3.0;
  for (float a = a0; a < a1; a += STEP) {
    float b = min(a + STEP, a1);
    float ra = a * DEG_TO_RAD, rb = b * DEG_TO_RAD;
    int x0 = CX + RING_OUTER * sin(ra), y0 = CY - RING_OUTER * cos(ra);
    int x1 = CX + RING_INNER * sin(ra), y1 = CY - RING_INNER * cos(ra);
    int x2 = CX + RING_OUTER * sin(rb), y2 = CY - RING_OUTER * cos(rb);
    int x3 = CX + RING_INNER * sin(rb), y3 = CY - RING_INNER * cos(rb);
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
    tft.fillTriangle(x1, y1, x3, y3, x2, y2, color);
  }
}

// Ring shows time remaining: full right after feeding, empty when it's time
void drawRing(int pct, uint16_t color) {
  if (pct == shownRingPct && color == shownRingColor) return;
  float split = 360.0 * pct / 100.0;
  if (split > 0)   drawArc(0, split, color);
  if (split < 360) drawArc(split, 360, COLOR_TRACK);
  shownRingPct = pct;
  shownRingColor = color;
}

// Clears a horizontal band inside the ring, then prints centered text
void drawLine(Shown& shown, const String& text, int y, int size, uint16_t color) {
  if (shown.text == text && shown.color == color) return;

  int h = 8 * size;
  int dy = max(abs(y - CY), abs(y + h - CY));                  // farthest edge from center
  int half = (int)sqrt((float)(RING_INNER * RING_INNER - dy * dy)) - 4;
  tft.fillRect(CX - half, y, half * 2, h, COLOR_BG);

  tft.setTextSize(size);
  tft.setTextColor(color);
  int w = text.length() * 6 * size;                            // built-in font is 6x8 px
  tft.setCursor(CX - w / 2, y);
  tft.print(text);

  shown.text = text;
  shown.color = color;
}

void drawFish(uint16_t color) {
  if ((int)color == shownFishColor) return;
  tft.fillRect(CX - 30, 46, 64, 28, COLOR_BG);
  tft.fillRoundRect(CX - 24, 49, 40, 22, 11, color);         // body
  tft.fillTriangle(CX + 12, 60, CX + 30, 48, CX + 30, 72, color);  // tail
  if (color != COLOR_BG) tft.fillCircle(CX - 13, 57, 3, COLOR_BG);  // eye
  shownFishColor = color;
}

// ---------- Screen ----------

void updateScreen() {
  if (lastFed() == 0) {
    drawRing(0, COLOR_TRACK);
    drawFish(blinkOn ? COLOR_ORANGE : COLOR_BG);
    drawLine(shownBig, "Not fed", 98, 3, COLOR_ORANGE);
    drawLine(shownLabel, "press button", 136, 2, COLOR_DIM);
    drawLine(shownLast, "", 170, 2, COLOR_DIM);
    return;
  }

  if (!timeIsSynced()) {
    drawRing(0, COLOR_TRACK);
    drawFish(COLOR_DIM);
    drawLine(shownBig, "...", 98, 3, COLOR_DIM);
    drawLine(shownLabel, "syncing clock", 136, 2, COLOR_DIM);
    drawLine(shownLast, "", 170, 2, COLOR_DIM);
    return;
  }

  long elapsed = (long)(time(nullptr) - lastFed());
  long remaining = (long)intervalSec() - elapsed;
  String lastText = "fed " + formatTime(lastFed(), "%I:%M %p");

  if (remaining <= 0) {
    drawRing(100, COLOR_ORANGE);
    drawFish(blinkOn ? COLOR_ORANGE : COLOR_BG);
    drawLine(shownBig, "Feed me!", 98, 3, COLOR_ORANGE);
    drawLine(shownLabel, "late " + formatCountdown(-remaining), 136, 2, COLOR_DIM);
    drawLine(shownLast, lastText, 170, 2, COLOR_DIM);
  } else {
    int pct = (int)(remaining * 100 / (long)intervalSec());
    drawRing(pct, COLOR_GREEN);
    drawFish(COLOR_GREEN);
    drawLine(shownBig, formatCountdown(remaining), 96, 4, COLOR_TEXT);
    drawLine(shownLabel, "until feeding", 136, 2, COLOR_DIM);
    drawLine(shownLast, lastText, 170, 2, COLOR_DIM);
  }
}

void showMessage(const String& line1, const String& line2) {
  drawLine(shownBig, line1, 98, 3, COLOR_TEXT);
  drawLine(shownLabel, line2, 136, 2, COLOR_DIM);
}

// ---------- Web page ----------

void handleRoot() {
  String status, detail, color;

  if (lastFed() == 0) {
    status = "Not fed yet";
    detail = "Press the button when you feed them.";
    color = "#d9822b";
  } else if (!timeIsSynced()) {
    status = "Syncing clock...";
    detail = "Refresh in a few seconds.";
    color = "#888888";
  } else {
    long ago = (long)(time(nullptr) - lastFed());
    status = needsFeeding() ? "Time to feed!" : "Fish are fed";
    detail = "Last fed " + formatAgo(ago);
    color = needsFeeding() ? "#d9822b" : "#2e9e6b";
  }

  String rows = "";
  if (timeIsSynced()) {
    for (int i = 0; i < HISTORY_SIZE; i++) {
      if (history[i] == 0) break;
      rows += "<li>" + formatTime((time_t)history[i], "%a %b %d, %I:%M %p") + "</li>";
    }
  }
  if (rows == "") rows = "<li>No feedings yet</li>";

  String html =
    "<!doctype html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<meta http-equiv='refresh' content='30'>"
    "<title>Fish Feeder</title>"
    "<style>"
    "body{font-family:-apple-system,system-ui,sans-serif;margin:0;padding:24px;"
    "background:#f4f7f6;color:#1d2b28;text-align:center}"
    ".card{max-width:360px;margin:24px auto;background:#fff;border-radius:16px;"
    "padding:28px;box-shadow:0 2px 12px rgba(0,0,0,.08)}"
    ".status{font-size:28px;font-weight:700;color:" + color + ";margin:8px 0 12px}"
    ".detail{font-size:18px;margin-bottom:24px}"
    "button{font-size:18px;padding:14px 24px;border:none;border-radius:12px;"
    "background:#2e9e6b;color:#fff;width:100%}"
    "h2{font-size:16px;margin:0 0 8px;color:#6b7a77;font-weight:600}"
    "ul{list-style:none;padding:0;margin:0;text-align:left}"
    "li{padding:8px 0;border-bottom:1px solid #eef2f1}"
    "li:last-child{border-bottom:none}"
    "</style></head><body>"
    "<div class='card'>"
    "<div style='font-size:40px'>&#128031;</div>"
    "<div class='status'>" + status + "</div>"
    "<div class='detail'>" + detail + "</div>"
    "<form method='POST' action='/fed'><button type='submit'>I just fed them</button></form>"
    "</div>"
    "<div class='card'><h2>Recent feedings</h2><ul>" + rows + "</ul></div>"
    "</body></html>";

  server.send(200, "text/html", html);
}

void handleFed() {
  recordFeeding();
  server.sendHeader("Location", "/");
  server.send(303);   // send the browser back to the main page
}

// ---------- Setup & loop ----------

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);   // pressed = LOW
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Screen
  tft.begin();
  COLOR_BG     = tft.color565(0, 0, 0);
  COLOR_TEXT   = tft.color565(255, 255, 255);
  COLOR_DIM    = tft.color565(140, 150, 148);
  COLOR_TRACK  = tft.color565(40, 44, 43);
  COLOR_GREEN  = tft.color565(46, 190, 120);
  COLOR_ORANGE = tft.color565(245, 140, 50);
  tft.fillScreen(COLOR_BG);
  drawRing(0, COLOR_TRACK);
  showMessage("Hello!", "connecting wifi");

  // Load saved feedings
  prefs.begin("fishfeeder", false);
  loadHistory();

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! Open http://");
    Serial.println(WiFi.localIP());
    showMessage("Online", WiFi.localIP().toString());   // IP stays up for 3 seconds
    delay(3000);
  } else {
    Serial.println("Couldn't connect. Double-check the Wi-Fi name/password (2.4 GHz only).");
    showMessage("No WiFi", "check password");
    delay(3000);
  }

  // Get the real time from the internet
  configTzTime(TIMEZONE, "pool.ntp.org", "time.nist.gov");

  // Lets you use http://fishfeeder.local on most phones and computers
  if (MDNS.begin(HOSTNAME)) {
    Serial.println("Or try http://fishfeeder.local");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/fed", HTTP_POST, handleFed);
  server.begin();
}

unsigned long lastScreenMs = 0;

void loop() {
  server.handleClient();

  // Debounced button read
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) {
    lastChangeMs = millis();
    lastReading = reading;
  }
  if (millis() - lastChangeMs > DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) {
      recordFeeding();
    }
  }

  // Blink the LED (and the fish on screen) when it's feeding time
  if (millis() - lastBlinkMs > 500) {
    lastBlinkMs = millis();
    if (needsFeeding()) {
      blinkOn = !blinkOn;
    } else {
      blinkOn = false;
    }
    digitalWrite(LED_PIN, blinkOn ? HIGH : LOW);
  }

  // Refresh the screen a few times a second (only changed parts get redrawn)
  if (millis() - lastScreenMs > 250) {
    lastScreenMs = millis();
    updateScreen();
  }
}
