#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <WebServer.h>
#include <qrcode.h>
#include <ArduinoJson.h>
#include <vector>

#include "FS.h"
#include "SD_MMC.h"

// Define camera model
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// OLED I2C GPIOs
#define OLED_SDA 1
#define OLED_SCL 2

// Shutter button stays on GPIO 0.
// The three extra buttons are a dedicated nav/action set.
#define BUTTON_PIN 0
#define UP_BUTTON_PIN 14
#define DOWN_BUTTON_PIN 10
#define ACTION_BUTTON_PIN 13
#define MENU_LONG_PRESS_MS 900

// MicroSD MMC Pins for ESP32-S3
#define SD_MMC_CLK 39
#define SD_MMC_CMD 38
#define SD_MMC_D0  40

// Upscaling and Pixelation Settings
#define PIXEL_BLOCK_SIZE 3
#define UPSCALE_FACTOR 6

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);

const char *ssid = "ESP32-CAM-S3";
const char *password = "12345678";

unsigned long lastShutterDebounceTime = 0;
unsigned long lastUpDebounceTime = 0;
unsigned long lastDownDebounceTime = 0;
unsigned long lastActionDebounceTime = 0;
const unsigned long debounceDelay = 200;
int pictureNumber = 0;

enum MenuScreen {
  ScreenCamera,
  ScreenMainMenu,
  ScreenFilterList
};

MenuScreen currentScreen = ScreenCamera;
int mainMenuIndex = 0;
int filterMenuIndex = 0;
bool wifiEnabled = true;
String activeFilterName = "None";
std::vector<String> paletteNames;

String getPaletteFilePath() {
  return "/www/palettes.json";
}

String getPaletteFallbackPath() {
  return "/palettes.json";
}

String readTextFile(const String &path) {
  if (!SD_MMC.exists(path)) {
    return "";
  }

  File file = SD_MMC.open(path, FILE_READ);
  if (!file) {
    return "";
  }

  String output = "";
  while (file.available()) {
    output += (char)file.read();
  }
  file.close();
  return output;
}

bool writeTextFile(const String &path, const String &content) {
  String folder = path.substring(0, path.lastIndexOf('/'));
  if (folder.length() > 0 && !SD_MMC.exists(folder)) {
    SD_MMC.mkdir(folder.c_str());
  }

  File file = SD_MMC.open(path.c_str(), FILE_WRITE);
  if (!file) {
    return false;
  }

  file.print(content);
  file.close();
  return true;
}

String getPaletteJsonFromDisk() {
  String primary = getPaletteFilePath();
  String fallback = getPaletteFallbackPath();

  if (SD_MMC.exists(primary)) {
    return readTextFile(primary);
  }

  if (SD_MMC.exists(fallback)) {
    String json = readTextFile(fallback);
    if (json.length() > 0) {
      writeTextFile(primary, json);
      return json;
    }
  }

  String emptyJson = "{\"palettes\":[ ]}";
  writeTextFile(primary, emptyJson);
  return emptyJson;
}

bool savePaletteJson(const String &json) {
  String primary = getPaletteFilePath();
  return writeTextFile(primary, json);
}

std::vector<String> extractPaletteNames(const String &json) {
  std::vector<String> names;
  DynamicJsonDocument doc(16384);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    return names;
  }

  JsonArray palettes = doc["palettes"];
  if (!palettes.is<JsonArray>()) {
    return names;
  }

  for (JsonVariant palette : palettes) {
    if (!palette.is<JsonObject>()) {
      continue;
    }

    const char *name = palette["name"] | "";
    if (name && strlen(name) > 0) {
      names.push_back(String(name));
    }
  }

  return names;
}

void refreshPaletteNames() {
  paletteNames = extractPaletteNames(getPaletteJsonFromDisk());
  if (paletteNames.empty()) {
    activeFilterName = "None";
  } else if (activeFilterName == "None" || activeFilterName == "") {
    activeFilterName = paletteNames[0];
  }
}

void initSDCard() {
  Serial.println("Initializing MicroSD Card...");
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);

  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Card Mount Failed!");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD Card attached.");
    return;
  }

  if (!SD_MMC.exists("/www")) {
    SD_MMC.mkdir("/www");
  }

  Serial.printf("SD Card Mounted. Size: %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));
}

int findNextPictureNumber() {
  int maxNumber = 0;
  File root = SD_MMC.open("/");

  if (root) {
    File file = root.openNextFile();
    while (file) {
      String name = String(file.name());
      if (name.startsWith("pixless_") && name.endsWith(".jpg")) {
        int numStart = String("pixless_").length();
        int numEnd = name.length() - String(".jpg").length();
        int currentNum = name.substring(numStart, numEnd).toInt();

        if (currentNum > maxNumber) {
          maxNumber = currentNum;
        }
      }
      file = root.openNextFile();
    }
    root.close();
  }

  return maxNumber;
}

void qrCodeDisplayCallback(esp_qrcode_handle_t qrcode) {
  int qrSize = esp_qrcode_get_size(qrcode);

  display.clearDisplay();

  int scale = 2;
  int xOffset = 2;
  int yOffset = 3;

  for (int y = 0; y < qrSize; y++) {
    for (int x = 0; x < qrSize; x++) {
      if (esp_qrcode_get_module(qrcode, x, y)) {
        display.fillRect(xOffset + (x * scale), yOffset + (y * scale), scale, scale, SH110X_WHITE);
      }
    }
  }

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(64, 4);
  display.println("Scan Web");
  display.setCursor(64, 18);
  display.println("IP:");
  display.setCursor(64, 28);
  display.println("192.168.4.1");
  display.setCursor(64, 42);
  display.println("AP:");
  display.setCursor(64, 52);
  display.println("ESP32-CAM");

  display.display();
}

void drawQRCodeAndInfo(const char *url) {
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  cfg.display_func = qrCodeDisplayCallback;
  esp_qrcode_generate(&cfg, url);
}

String getContentType(String filename) {
  if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ttf")) return "font/ttf";
  else if (filename.endsWith(".woff")) return "font/woff";
  else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

bool handleSDFileRead(String path) {
  String webRoot = "/www";

  if (path.endsWith("/")) {
    path = "/html/main.html";
  }

  String fullPath = webRoot + path;
  String contentType = getContentType(path);

  if (SD_MMC.exists(fullPath)) {
    File file = SD_MMC.open(fullPath, FILE_READ);
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void renderCameraScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Camera Mode");
  display.setCursor(0, 14);
  display.println("WiFi:");
  display.setCursor(42, 14);
  display.println(wifiEnabled ? "ON" : "OFF");
  display.setCursor(0, 28);
  display.println("Filter:");
  display.setCursor(48, 28);
  display.println(activeFilterName.substring(0, min(10, activeFilterName.length())));
  display.setCursor(0, 46);
  display.println("Hold to menu");
  display.display();
}

void renderMainMenu() {
  const char *items[] = {"WiFi QR", "Filters", "Camera"};

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Menu");

  for (int i = 0; i < 3; i++) {
    display.setCursor(0, 14 + (i * 14));
    if (i == mainMenuIndex) {
      display.print("> ");
    }
    display.println(items[i]);
  }

  display.setCursor(0, 52);
  display.println(wifiEnabled ? "WiFi ON" : "WiFi OFF");
  display.display();
}

void renderFilterMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Filters");

  int count = paletteNames.size();
  for (int i = 0; i < 4; i++) {
    int index = filterMenuIndex - 1 + i;
    if (index < 0 || index >= count) {
      continue;
    }

    display.setCursor(0, 14 + (i * 12));
    if (index == filterMenuIndex) {
      display.print("> ");
    }
    display.println(paletteNames[index]);
  }

  display.setCursor(0, 52);
  if (count > 0) {
    display.println(String("Sel: ") + paletteNames[filterMenuIndex]);
  } else {
    display.println("No filters");
  }

  display.display();
}

void showCameraOrMenu() {
  if (currentScreen == ScreenCamera) {
    renderCameraScreen();
  } else if (currentScreen == ScreenMainMenu) {
    renderMainMenu();
  } else {
    renderFilterMenu();
  }
}

void openMenu() {
  currentScreen = ScreenMainMenu;
  mainMenuIndex = 0;
  showCameraOrMenu();
}

void closeMenu() {
  currentScreen = ScreenCamera;
  showCameraOrMenu();
}

void toggleWifi() {
  wifiEnabled = !wifiEnabled;
  if (wifiEnabled) {
    WiFi.softAP(ssid, password);
    drawQRCodeAndInfo("http://192.168.4.1");
  } else {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    renderCameraScreen();
  }
}

void selectMainMenu() {
  if (mainMenuIndex == 0) {
    toggleWifi();
  } else if (mainMenuIndex == 1) {
    if (paletteNames.empty()) {
      currentScreen = ScreenFilterList;
      filterMenuIndex = 0;
      showCameraOrMenu();
      return;
    }
    filterMenuIndex = 0;
    currentScreen = ScreenFilterList;
  } else {
    currentScreen = ScreenCamera;
  }
  showCameraOrMenu();
}

void selectFilter() {
  if (paletteNames.empty()) {
    return;
  }
  activeFilterName = paletteNames[filterMenuIndex];
  currentScreen = ScreenCamera;
  showCameraOrMenu();
}

void onUpButton() {
  if (currentScreen == ScreenMainMenu) {
    mainMenuIndex = (mainMenuIndex + 2) % 3;
  } else if (currentScreen == ScreenFilterList) {
    if (!paletteNames.empty()) {
      filterMenuIndex = (filterMenuIndex + 1) % paletteNames.size();
    }
  }
  showCameraOrMenu();
}

void onDownButton() {
  if (currentScreen == ScreenMainMenu) {
    mainMenuIndex = (mainMenuIndex + 1) % 3;
  } else if (currentScreen == ScreenFilterList) {
    if (!paletteNames.empty()) {
      filterMenuIndex = (filterMenuIndex + paletteNames.size() - 1) % paletteNames.size();
    }
  }
  showCameraOrMenu();
}

void handleActionPress(bool longPress) {
  if (longPress) {
    if (currentScreen == ScreenCamera) {
      openMenu();
    } else {
      closeMenu();
    }
    return;
  }

  if (currentScreen == ScreenCamera) {
    openMenu();
  } else if (currentScreen == ScreenMainMenu) {
    selectMainMenu();
  } else if (currentScreen == ScreenFilterList) {
    selectFilter();
  }
}

void handleMenuButtons() {
  static bool actionWasDown = false;
  static unsigned long actionPressStartedAt = 0;

  bool upPressed = (digitalRead(UP_BUTTON_PIN) == LOW);
  bool downPressed = (digitalRead(DOWN_BUTTON_PIN) == LOW);
  bool actionPressed = (digitalRead(ACTION_BUTTON_PIN) == LOW);

  if (upPressed && (millis() - lastUpDebounceTime > debounceDelay)) {
    lastUpDebounceTime = millis();
    onUpButton();
  }

  if (downPressed && (millis() - lastDownDebounceTime > debounceDelay)) {
    lastDownDebounceTime = millis();
    onDownButton();
  }

  if (actionPressed && !actionWasDown && (millis() - lastActionDebounceTime > debounceDelay)) {
    lastActionDebounceTime = millis();
    actionPressStartedAt = millis();
  }

  if (!actionPressed && actionWasDown) {
    unsigned long heldFor = millis() - actionPressStartedAt;
    handleActionPress(heldFor >= MENU_LONG_PRESS_MS);
    lastActionDebounceTime = millis();
  }

  actionWasDown = actionPressed;
}

void handleSetVar() {
  String var = server.arg("var");
  int val = server.arg("val").toInt();

  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    server.send(500, "text/plain", "Sensor Error");
    return;
  }

  int res = -1;
  if (var == "brightness") res = s->set_brightness(s, val);
  else if (var == "contrast") res = s->set_contrast(s, val);
  else if (var == "saturation") res = s->set_saturation(s, val);

  if (res == 0) server.send(200, "text/plain", "OK");
  else server.send(500, "text/plain", "Set Failed");
}

void handleFile() {
  String filename = "/" + server.arg("name");
  filename.replace("//", "/");

  if (!SD_MMC.exists(filename)) {
    server.send(404, "text/plain", "File Not Found");
    return;
  }

  File file = SD_MMC.open(filename, FILE_READ);
  size_t fileSize = file.size();

  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: image/jpeg");
  client.println("Content-Disposition: inline; filename=\"" + server.arg("name") + "\"");
  client.println("Content-Length: " + String(fileSize));
  client.println("Connection: close");
  client.println();

  uint8_t buf[2048];
  while (file.available()) {
    int bytesRead = file.read(buf, sizeof(buf));
    client.write(buf, bytesRead);
  }

  file.close();
}

void handleApiPhotos() {
  File root = SD_MMC.open("/");
  String json = "{\"photos\":[";
  bool first = true;
  int totalPhotos = 0;

  if (root) {
    File file = root.openNextFile();
    while (file) {
      String name = String(file.name());
      if (name.endsWith(".jpg")) {
        if (!first) json += ",";
        json += "\"" + name + "\"";
        first = false;
        totalPhotos++;
      }
      file = root.openNextFile();
    }
    root.close();
  }

  uint64_t totalMB = SD_MMC.cardSize() / (1024 * 1024);
  uint64_t usedMB = SD_MMC.usedBytes() / (1024 * 1024);

  json += "],\"total\":" + String(totalPhotos);
  json += ",\"used_mb\":" + String(usedMB);
  json += ",\"total_mb\":" + String(totalMB);
  json += "}";

  server.send(200, "application/json", json);
}

void handleApiPalettes() {
  String method = server.method() == HTTP_GET ? "GET" : server.method() == HTTP_POST ? "POST" : server.method() == HTTP_PUT ? "PUT" : "DELETE";

  if (method == "GET") {
    String json = getPaletteJsonFromDisk();
    server.send(200, "application/json", json);
    return;
  }

  String body = server.arg("plain");
  if (body.length() == 0) {
    body = server.arg("body");
  }

  if (method == "DELETE") {
    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, getPaletteJsonFromDisk());
    if (!err && doc["palettes"].is<JsonArray>()) {
      int index = server.arg("index").toInt();
      if (index >= 0 && index < doc["palettes"].size()) {
        JsonArray palettes = doc["palettes"].as<JsonArray>();
        palettes.remove(index);
      }
      String updated;
      serializeJson(doc, updated);
      if (savePaletteJson(updated)) {
        refreshPaletteNames();
        server.send(200, "application/json", updated);
        return;
      }
    }
    server.send(400, "text/plain", "Invalid palette delete payload");
    return;
  }

  if (body.length() == 0) {
    server.send(400, "text/plain", "Missing palette payload");
    return;
  }

  DynamicJsonDocument input(16384);
  DeserializationError err = deserializeJson(input, body);
  if (err) {
    server.send(400, "text/plain", "Invalid JSON payload");
    return;
  }

  DynamicJsonDocument document(16384);
  DeserializationError loadErr = deserializeJson(document, getPaletteJsonFromDisk());
  if (loadErr) {
    document.clear();
    document["palettes"] = JsonArray();
  }

  JsonArray src = input["palettes"] | JsonArray();
  if (src.isNull() || src.size() == 0) {
    if (input["name"].is<String>() && input["colors"].is<JsonArray>()) {
      JsonArray newPalettes = document.createNestedArray("palettes");
      JsonObject newPalette = newPalettes.add<JsonObject>();
      newPalette["name"] = input["name"].as<String>();
      JsonArray colors = newPalette.createNestedArray("colors");
      for (JsonVariant v : input["colors"].as<JsonArray>()) {
        colors.add(v.as<String>());
      }
    } else {
      server.send(400, "text/plain", "Palette payload is invalid");
      return;
    }
  } else {
    JsonArray newPalettes = document.createNestedArray("palettes");
    for (JsonVariant palette : src) {
      JsonObject item = newPalettes.add<JsonObject>();
      item["name"] = palette["name"].as<String>();
      JsonArray colors = item.createNestedArray("colors");
      for (JsonVariant color : palette["colors"].as<JsonArray>()) {
        colors.add(color.as<String>());
      }
    }
  }

  String updatedJson;
  serializeJson(document, updatedJson);
  if (!savePaletteJson(updatedJson)) {
    server.send(500, "text/plain", "Failed to write palette file");
    return;
  }

  refreshPaletteNames();
  server.send(200, "application/json", updatedJson);
}

void startCustomCameraServer() {
  server.on("/set", handleSetVar);
  server.on("/file", handleFile);
  server.on("/api/photos", handleApiPhotos);
  server.on("/api/palettes", HTTP_GET, handleApiPalettes);
  server.on("/api/palettes", HTTP_POST, handleApiPalettes);
  server.on("/api/palettes", HTTP_PUT, handleApiPalettes);
  server.on("/api/palettes", HTTP_DELETE, handleApiPalettes);
  server.on("/palettes.json", HTTP_GET, []() {
    server.send(200, "application/json", getPaletteJsonFromDisk());
  });

  server.onNotFound([]() {
    if (!handleSDFileRead(server.uri())) {
      server.send(404, "text/plain", "404: File Not Found");
    }
  });

  server.begin();
  Serial.println("HTTP Web Server started");
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(UP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DOWN_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ACTION_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL, 400000);
  if (!display.begin(SCREEN_ADDRESS, true)) {
    Serial.println(F("SH110X initialization failed!"));
  } else {
    display.setRotation(0);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Pixless Camera");
    display.println("Mounting SD card...");
    display.display();
  }

  initSDCard();
  refreshPaletteNames();

  pictureNumber = findNextPictureNumber();
  Serial.printf("Found highest existing photo index: %d. Next capture will be %d.\n", pictureNumber, pictureNumber + 1);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;

  config.frame_size = FRAMESIZE_HQVGA;
  config.pixel_format = PIXFORMAT_RGB565;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 0);
  s->set_brightness(s, 0);
  s->set_contrast(s, 2);
  s->set_saturation(s, 4);

  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access Point IP address: ");
  Serial.println(IP);

  startCustomCameraServer();
  showCameraOrMenu();
}

void loop() {
  server.handleClient();
  handleMenuButtons();

  if (digitalRead(BUTTON_PIN) == LOW && (millis() - lastShutterDebounceTime > debounceDelay)) {
    lastShutterDebounceTime = millis();

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      captureAndSavePixelatedImage(fb);
      esp_camera_fb_return(fb);
    } else {
      Serial.println("Camera frame capture failed!");
    }
  }

  static unsigned long lastMenuRefresh = 0;
  if (millis() - lastMenuRefresh > 5000) {
    lastMenuRefresh = millis();
    refreshPaletteNames();
    showCameraOrMenu();
  }
}

