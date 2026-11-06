#include <Adafruit_NeoPixel.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_wifi.h>
#include <atomic>
#include <iterator>

#include "BleControl.h"
#include "LedState.h"
#include "WebUi.h"

constexpr uint8_t LED_PIN = 32;  // WS2812B DIN on the Grove port.
constexpr uint8_t INDICATOR_PIN = 27;
constexpr uint8_t BUTTON_PIN = 39;
constexpr uint32_t MODE_HOLD_MS = 500;
constexpr uint32_t BUTTON_HOLD_MS = 1500;
constexpr uint32_t WIFI_STARTUP_TIMEOUT_MS = 15000;
constexpr uint32_t RECONNECT_INTERVAL_MS = 15000;
constexpr char API_PATH[] = "/api/state";
constexpr char CONTROL_HEADER[] = "X-Atom-Control";

Adafruit_NeoPixel strip(LED_COUNT_MAX, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel indicator(1, INDICATOR_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
WiFiManager wifiManager;
LedState state = DEFAULT_STATE;
QueueHandle_t renderCommands = nullptr;
std::atomic<uint32_t> indicatorColor{0};
bool wifiMode = false;
bool webRunning = false;
bool wifiConfigured = false;
bool checkingSavedWifi = false;
bool portalActive = false;
uint32_t lastReconnect = 0;
String webAddress;
char deviceName[24];

void renderTask(void *) {
  LedState active = DEFAULT_STATE;
  uint32_t patternStart = millis();
  uint32_t previousColor = 0;
  uint8_t previousCount = active.ledCount;
  uint32_t previousIndicator = 0;
  for (;;) {
    const uint32_t now = millis();
    LedState command;
    if (xQueueReceive(renderCommands, &command, 0) == pdTRUE) {
      if (active.pattern != command.pattern || active.speed != command.speed) patternStart = now;
      active = command;
    }
    const uint32_t elapsed = now - patternStart;
    const uint8_t level = frameBrightness(active, elapsed);
    const uint32_t color = active.pattern == Rainbow
        ? strip.ColorHSV(rainbowHue(elapsed, active.speed), 255, level)
        : strip.Color(active.red * level / 255, active.green * level / 255,
                      active.blue * level / 255);
    if (color != previousColor || active.ledCount != previousCount) {
      strip.clear();
      strip.fill(color, 0, active.ledCount);
      strip.show();
      previousColor = color;
      previousCount = active.ledCount;
    }
    const uint32_t nextIndicator = indicatorColor;
    if (nextIndicator != previousIndicator) {
      indicator.setPixelColor(0, nextIndicator);
      indicator.show();
      previousIndicator = nextIndicator;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void applyState(const LedState &command) {
  state = command;
  xQueueOverwrite(renderCommands, &state);
}

int hexDigit(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
  if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
  return -1;
}

bool parseHttpState(const String &body, LedState &command) {
  if (body.length() != 2 * STATE_SIZE && body.length() != 2 * LEGACY_STATE_SIZE) return false;
  const size_t size = body.length() / 2;
  uint8_t bytes[STATE_SIZE];
  for (size_t i = 0; i < size; ++i) {
    const int high = hexDigit(body[2 * i]);
    const int low = hexDigit(body[2 * i + 1]);
    if (high < 0 || low < 0) return false;
    bytes[i] = high * 16 + low;
  }
  return decodeState(bytes, size, command);
}

bool requireLocalAddress() {
  const String host = server.hostHeader();
  if (host == webAddress || host == webAddress + ":80") return true;
  server.send(403, "text/plain", "Open the device IP address");
  return false;
}

void sendState() {
  constexpr char hexChars[] = "0123456789abcdef";
  uint8_t bytes[STATE_SIZE];
  char body[2 * STATE_SIZE + 1] = {};
  encodeState(state, bytes);
  for (size_t i = 0; i < STATE_SIZE; ++i) {
    body[2 * i] = hexChars[bytes[i] >> 4];
    body[2 * i + 1] = hexChars[bytes[i] & 15];
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", body);
}

void configureWeb() {
  const char *headers[] = {CONTROL_HEADER};
  server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, [] {
    if (!requireLocalAddress()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", UI_PAGE);
  });
  server.on(API_PATH, HTTP_GET, [] {
    if (requireLocalAddress()) sendState();
  });
  server.on(API_PATH, HTTP_PUT, [] {
    if (!requireLocalAddress()) return;
    if (server.header(CONTROL_HEADER) != "1") {
      server.send(403, "text/plain", "Control header required");
      return;
    }
    LedState command = state;
    if (!parseHttpState(server.arg("plain"), command)) {
      server.send(400, "text/plain", "Invalid LED state");
      return;
    }
    applyState(command);
    sendState();
  });
}

void stopWeb() {
  if (webRunning) {
    server.stop();
    webRunning = false;
  }
}

void resumeStation() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  wifi_config_t config{};
  wifiConfigured = esp_wifi_get_config(WIFI_IF_STA, &config) == ESP_OK && config.sta.ssid[0] != 0;
  if (wifiConfigured && WiFi.status() != WL_CONNECTED) WiFi.begin();
  lastReconnect = millis();
}

void startSetupPortal() {
  if (portalActive) return;
  checkingSavedWifi = false;
  stopWeb();
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  wifiManager.startConfigPortal(deviceName);
  // WiFiManager sets its active flag even if SoftAP startup fails.
  portalActive = wifiManager.getConfigPortalActive() && WiFi.softAPSSID() == deviceName
                 && WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
  if (!portalActive) {
    if (wifiManager.getConfigPortalActive()) wifiManager.stopConfigPortal();
    Serial.println("Wi-Fi setup AP could not start.");
    resumeStation();
    return;
  }
  wifiManager.server->removeRoute("/update", HTTP_GET);
  wifiManager.server->removeRoute("/u", HTTP_POST);
  wifiManager.server->removeRoute("/erase");
  wifiManager.server->removeRoute("/restart");
  Serial.printf("Join setup AP %s and open http://192.168.4.1/\n", deviceName);
}

void beginWifi() {
  WiFi.setHostname(deviceName);
  wifiManager.setDebugOutput(false);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setAPClientCheck(false);
  wifiManager.setWebPortalClientCheck(false);
  const char *setupMenu[] = {"wifi", "exit"};
  wifiManager.setMenu(setupMenu, std::size(setupMenu));
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setShowInfoErase(false);
  wifiManager.setWiFiAutoReconnect(false);
  wifiManager.setConnectTimeout(8);
  wifiManager.setSaveConnectTimeout(8);
  wifiManager.setWebServerCallback([] {
    wifiManager.server->addMiddleware([](WebServer &portal, Middleware::Callback next) {
      if (portal.client().localIP() == WiFi.softAPIP()) return next();
      portal.send(403, "text/plain", "Use the setup AP");
      return false;
    });
  });
  configureWeb();
  resumeStation();
  checkingSavedWifi = wifiConfigured;
  if (!checkingSavedWifi) startSetupPortal();
}

void tickWifi() {
  uint32_t now = millis();
  if (portalActive) {
    wifiManager.process();
    portalActive = wifiManager.getConfigPortalActive();
    if (portalActive) return;
    resumeStation();
    now = millis();
  }
  if (WiFi.status() != WL_CONNECTED) {
    stopWeb();
    if (checkingSavedWifi) {
      if (now - lastReconnect >= WIFI_STARTUP_TIMEOUT_MS) startSetupPortal();
      return;
    }
    if (wifiConfigured && now - lastReconnect >= RECONNECT_INTERVAL_MS) {
      lastReconnect = now;
      WiFi.reconnect();
    }
    return;
  }
  checkingSavedWifi = false;
  const String address = WiFi.localIP().toString();
  if (!webRunning || address != webAddress) {
    stopWeb();
    webAddress = address;
    server.begin();
    webRunning = true;
    Serial.printf("Open http://%s/\n", webAddress.c_str());
  }
  server.handleClient();
}

bool heldAtStartup() {
  const uint32_t started = millis();
  while (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - started >= MODE_HOLD_MS) return true;
    delay(5);
  }
  return false;
}

void checkButton(uint32_t now) {
  static bool armed = false;  // Startup press must be released before runtime actions.
  static uint32_t downSince = 0;
  if (digitalRead(BUTTON_PIN) == HIGH) {
    armed = true;
    downSince = now;
  } else if (armed && now - downSince >= BUTTON_HOLD_MS) {
    armed = false;
    if (wifiMode) startSetupPortal();
    else BleControl::openWindow();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);
  wifiMode = heldAtStartup();
  strip.begin();
  strip.clear();
  strip.show();
  indicator.begin();
  indicator.clear();
  indicator.show();
  renderCommands = xQueueCreate(1, sizeof(LedState));
  if (!renderCommands || xTaskCreate(renderTask, "atom-led", 3072, nullptr, 1, nullptr) != pdPASS) {
    Serial.println("Could not start LED rendering.");
    for (;;) delay(1000);
  }
  snprintf(deviceName, sizeof(deviceName), "AtomPixel-%06lX",
           static_cast<unsigned long>((ESP.getEfuseMac() >> 24) & 0xffffff));

  if (wifiMode) {
    Serial.println("Mode: Wi-Fi");
    beginWifi();
  } else {
    Serial.println("Mode: BLE. Hold button to allow connection for 60 seconds.");
    BleControl::begin(deviceName, state);
  }
}

void loop() {
  const uint32_t now = millis();
  checkButton(now);
  if (wifiMode) {
    tickWifi();
    if (portalActive) indicatorColor = 0x060008;  // Violet
    else indicatorColor = webRunning ? 0x000800 : 0x000008;
  } else {
    BleControl::tick();
    LedState command;
    if (BleControl::takeCommand(command)) {
      applyState(command);
    }
    indicatorColor = BleControl::isConnected() ? 0x000800
                     : BleControl::windowOpen() ? 0x000808 : 0x000005;
  }
  delay(2);
}
