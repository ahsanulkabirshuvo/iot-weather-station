/*
  IoT Weather Station - NodeMCU (ESP8266)

  Measures temperature, humidity, air pressure, light level and rain, then
    1. shows the values on a built-in web page (http://<device-ip>/) and as JSON (/json), and
    2. optionally sends them to a Blynk IoT dashboard.

  Sensors
    DHT11    temperature and humidity   digital, pin D5
    BMP180   air pressure               I2C, SDA = D2, SCL = D1
    LDR      light level                analog, pin A0 (voltage divider)
    Rain     rain / no rain             digital output (DO) of the rain module, pin D6

  Board:     NodeMCU 1.0 (ESP-12E Module)
  Libraries: DHT sensor library (Adafruit), Adafruit Unified Sensor,
             Adafruit BMP085 Library (works with BMP180), Blynk (only if USE_BLYNK is set)

  Setup: copy config.example.h to config.h and fill in your Wi-Fi (and Blynk) details.
*/

#if __has_include("config.h")
  #include "config.h"
#else
  #error "config.h not found. Copy config.example.h to config.h and fill in your details."
#endif

#ifdef USE_BLYNK
  #define BLYNK_PRINT Serial
#endif

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>
#ifdef USE_BLYNK
  #include <BlynkSimpleEsp8266.h>
#endif

// ---------------------------------------------------------------- Pin map
const uint8_t PIN_DHT  = D5;   // DHT11 data
const uint8_t PIN_RAIN = D6;   // rain module digital output (DO)
const uint8_t PIN_SDA  = D2;   // BMP180 SDA
const uint8_t PIN_SCL  = D1;   // BMP180 SCL
const uint8_t PIN_LDR  = A0;   // LDR divider: 3.3V - LDR - A0 - 10k - GND

// ---------------------------------------------------------------- Settings
const bool RAIN_ACTIVE_LOW = true;    // most rain modules pull DO low when wet
const bool LDR_INVERT      = false;   // set true if your LDR is wired the other way round

const unsigned long READ_INTERVAL_MS = 5000;    // DHT11 needs at least 1 s between reads
const unsigned long WIFI_TIMEOUT_MS  = 20000;   // how long to wait for Wi-Fi at start-up
const unsigned long BLYNK_RETRY_MS   = 15000;   // how often to retry the Blynk connection

// ---------------------------------------------------------------- Globals
DHT dht(PIN_DHT, DHT11);
Adafruit_BMP085 bmp;
ESP8266WebServer server(80);

struct Weather {
  float temperatureC = NAN;
  float humidity     = NAN;
  float pressureHpa  = NAN;
  int   lightPct     = 0;
  bool  raining      = false;
} wx;

bool bmpOk = false;
unsigned long lastRead = 0;
#ifdef USE_BLYNK
unsigned long lastBlynkTry = 0;
#endif

// ---------------------------------------------------------------- Web page
const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Weather Station</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;padding:16px;background:#0f172a;color:#e2e8f0}
h1{font-size:1.3rem;margin:0 0 16px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px}
.card{background:#1e293b;border-radius:12px;padding:16px}
.label{font-size:.8rem;color:#94a3b8}
.value{font-size:1.8rem;font-weight:600;margin-top:4px}
</style></head><body>
<h1>Weather Station</h1>
<div class="grid">
<div class="card"><div class="label">Temperature</div><div class="value" id="t">--</div></div>
<div class="card"><div class="label">Humidity</div><div class="value" id="h">--</div></div>
<div class="card"><div class="label">Pressure</div><div class="value" id="p">--</div></div>
<div class="card"><div class="label">Light</div><div class="value" id="l">--</div></div>
<div class="card"><div class="label">Rain</div><div class="value" id="r">--</div></div>
</div>
<p class="label" id="s">Loading...</p>
<script>
const $ = id => document.getElementById(id);
const fmt = (v, unit) => v === null ? 'n/a' : v + unit;
async function update() {
  try {
    const d = await (await fetch('/json')).json();
    $('t').textContent = fmt(d.temperature_c, ' \u00B0C');
    $('h').textContent = fmt(d.humidity_pct, ' %');
    $('p').textContent = fmt(d.pressure_hpa, ' hPa');
    $('l').textContent = fmt(d.light_pct, ' %');
    $('r').textContent = d.raining ? 'Raining' : 'Dry';
    $('s').textContent = 'Updated ' + new Date().toLocaleTimeString();
  } catch (e) {
    $('s').textContent = 'Connection lost - retrying...';
  }
}
update();
setInterval(update, 5000);
</script></body></html>)HTML";

// ---------------------------------------------------------------- Helpers
String jsonNumber(float value, unsigned int decimals) {
  return isnan(value) ? String("null") : String(value, decimals);
}

String buildJson() {
  String j = "{";
  j += "\"temperature_c\":" + jsonNumber(wx.temperatureC, 1);
  j += ",\"humidity_pct\":" + jsonNumber(wx.humidity, 0);
  j += ",\"pressure_hpa\":" + jsonNumber(wx.pressureHpa, 1);
  j += ",\"light_pct\":" + String(wx.lightPct);
  j += ",\"raining\":" + String(wx.raining ? "true" : "false");
  j += "}";
  return j;
}

int readLightPercent() {
  long sum = 0;
  for (int i = 0; i < 8; i++) {          // average a few samples to smooth out noise
    sum += analogRead(PIN_LDR);
    delay(2);
  }
  int pct = (int)(sum / 8 * 100 / 1023);
  if (LDR_INVERT) pct = 100 - pct;
  return constrain(pct, 0, 100);
}

void readSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    wx.temperatureC = t;
    wx.humidity = h;
  } else {
    Serial.println(F("DHT11 read failed - keeping the last good value."));
  }

  if (bmpOk) {
    wx.pressureHpa = bmp.readPressure() / 100.0f;   // Pa -> hPa (same as mbar)
  }

  wx.lightPct = readLightPercent();
  wx.raining = (digitalRead(PIN_RAIN) == (RAIN_ACTIVE_LOW ? LOW : HIGH));
}

void printReading() {
  Serial.printf("T = %.1f C | RH = %.0f %% | P = %.1f hPa | Light = %d %% | Rain = %s\n",
                wx.temperatureC, wx.humidity, wx.pressureHpa, wx.lightPct,
                wx.raining ? "yes" : "no");
}

#ifdef USE_BLYNK
// Virtual pins: V0 temperature, V1 humidity, V2 rain gauge (0/100), V3 pressure,
// V4 rain LED, V5 light level
void sendToBlynk() {
  if (!Blynk.connected()) return;
  if (!isnan(wx.temperatureC)) Blynk.virtualWrite(V0, wx.temperatureC);
  if (!isnan(wx.humidity))     Blynk.virtualWrite(V1, wx.humidity);
  Blynk.virtualWrite(V2, wx.raining ? 100 : 0);
  if (!isnan(wx.pressureHpa))  Blynk.virtualWrite(V3, wx.pressureHpa);
  Blynk.virtualWrite(V4, wx.raining ? 255 : 0);
  Blynk.virtualWrite(V5, wx.lightPct);
}
#endif

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print(F("Connecting to Wi-Fi"));
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("Connected. Open http://"));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("Wi-Fi not connected yet. Check the SSID and password in config.h."));
    Serial.println(F("The board keeps retrying in the background."));
  }
}

void handleRoot() { server.send_P(200, "text/html", PAGE); }
void handleJson() { server.send(200, "application/json", buildJson()); }

// ---------------------------------------------------------------- Arduino entry points
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("IoT Weather Station starting..."));

  pinMode(PIN_RAIN, INPUT);
  dht.begin();

  Wire.begin(PIN_SDA, PIN_SCL);
  bmpOk = bmp.begin();
  if (!bmpOk) {
    Serial.println(F("BMP180 not found - check wiring (SDA = D2, SCL = D1). Pressure disabled."));
  }

  connectWiFi();

#ifdef USE_BLYNK
  Blynk.config(BLYNK_AUTH_TOKEN);
#endif

  server.on("/", handleRoot);
  server.on("/json", handleJson);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();

  readSensors();
  printReading();
  lastRead = millis();
}

void loop() {
  server.handleClient();

#ifdef USE_BLYNK
  if (Blynk.connected()) {
    Blynk.run();
  } else if (WiFi.status() == WL_CONNECTED && millis() - lastBlynkTry >= BLYNK_RETRY_MS) {
    lastBlynkTry = millis();
    Blynk.connect(3000);
  }
#endif

  if (millis() - lastRead >= READ_INTERVAL_MS) {
    lastRead = millis();
    readSensors();
    printReading();
#ifdef USE_BLYNK
    sendToBlynk();
#endif
  }
}
