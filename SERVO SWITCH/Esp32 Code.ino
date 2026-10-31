#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

const char* WIFI_SSID  = "your_network_name";
const char* WIFI_PASS  = "your_net_pass";
const char* DEVICE_KEY = "mibrahim";
const char* BASE_URL   = "https://dawn-sound-e941.m-ibrahim-ibh.workers.dev";

const int SERVO_PIN    = 4;
const int CENTER_ANGLE = 90;    // bar level, not touching the switch
const int ON_ANGLE     = 40;    // pushes one side of the rocker
const int OFF_ANGLE    = 140;   // pushes the other side

Servo servo;
String lastId = "";
bool first = true;

void pushTo(int angle) {
  Serial.println("Servo -> " + String(angle));
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2500);
  servo.write(angle);
  delay(800);
  servo.write(CENTER_ANGLE);
  delay(800);
  servo.detach();
}

void scanWiFi() {
  Serial.println("Scanning WiFi...");
  int n = WiFi.scanNetworks();
  if (n <= 0) { Serial.println("No networks found"); return; }
  for (int i = 0; i < n; i++) {
    Serial.println(String(i + 1) + ": " + WiFi.SSID(i) + "  (" + WiFi.RSSI(i) + " dBm)");
  }
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to " + String(WIFI_SSID));
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - t > 20000) {
      Serial.println("\nWiFi FAILED. Check name/password, use 2.4 GHz.");
      return false;
    }
  }
  Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== ESP32 START ===");

  Serial.println("Servo self-test...");
  pushTo(ON_ANGLE);
  pushTo(OFF_ANGLE);

  scanWiFi();
  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (!connectWiFi()) { delay(5000); return; }
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);
  HTTPClient http;
  http.setTimeout(10000);
  String url = String(BASE_URL) + "/api/poll?key=" + DEVICE_KEY;

  if (http.begin(client, url)) {
    int code = http.GET();
    Serial.println("Poll HTTP code: " + String(code));
    if (code == 200) {
      String r = http.getString();
      Serial.println("Reply: " + r);
      int p = r.indexOf('|');
      if (p > 0) {
        String id  = r.substring(0, p);
        String act = r.substring(p + 1);
        if (id != lastId) {
          lastId = id;
          if (!first) {                       // ignore the old command on boot
            Serial.println("Command: " + act);
            if (act == "on") pushTo(ON_ANGLE);
            else if (act == "off") pushTo(OFF_ANGLE);
          }
        }
        first = false;
      }
    } else if (code == 401) {
      Serial.println("401: DEVICE_KEY does not match the Worker");
    } else if (code == 500) {
      Serial.println("500: KV binding missing/misnamed in the Worker");
    } else if (code < 0) {
      Serial.println("Connection error: " + http.errorToString(code));
    }
    http.end();
  } else {
    Serial.println("http.begin failed");
  }
  delay(3000);
}