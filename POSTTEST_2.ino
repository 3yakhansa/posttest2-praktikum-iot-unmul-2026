#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ─── CONFIGURE THESE ────────────────────────────────────────────────
const char* WIFI_SSID     = "Beli kuota aja sudah erna";
const char* WIFI_PASS     = "turuntangan";
const String BOT_TOKEN    = "7961567055:AAEUAPQx-e60aku6j61oTkX5_otA3iyfRMA";
const String CHAT_ID      = "-5244619827";  // negative number for groups

const String USER1_ID     = "1030884727";  // controls LED1
const String USER2_ID     = "6584550705";  // controls LED2
const String USER3_ID     = "5907638031";  // controls LED3

const int GAS_THRESHOLD   = 700;         // 0–4095 (12-bit ADC), tune this
// ────────────────────────────────────────────────────────────────────

// Pin definitions
#define LED1_PIN   7
#define LED2_PIN   8
#define LED3_PIN   9
#define LED4_PIN   10
#define MQ2_PIN    3
#define DHT_PIN    4
#define DHT_TYPE   DHT22

DHT dht(DHT_PIN, DHT_TYPE);

long lastUpdateId     = 0;
unsigned long lastGasCheck = 0;
const unsigned long GAS_INTERVAL = 5000; // check gas every 5s
bool gasAlertSent     = false;

// ─── TELEGRAM HELPERS ────────────────────────────────────────────────

void sendMessage(String chatId, String text) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(512);
  doc["chat_id"] = chatId;
  doc["text"]    = text;
  String body;
  serializeJson(doc, body);
  http.POST(body);
  http.end();
}

void getUpdates() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN +
               "/getUpdates?offset=" + String(lastUpdateId + 1) + "&timeout=1";
  http.begin(url);
  int code = http.GET();
  if (code != 200) { http.end(); return; }

  DynamicJsonDocument doc(4096);
  deserializeJson(doc, http.getString());
  http.end();

  JsonArray results = doc["result"].as<JsonArray>();
  for (JsonObject update : results) {
    lastUpdateId = update["update_id"].as<long>();

    // Only handle messages (ignore non-message updates)
    if (!update.containsKey("message")) continue;

    JsonObject msg     = update["message"];
    String fromId      = msg["from"]["id"].as<String>();
    String text        = msg["text"] | "";
    text.toLowerCase();
    text.trim();

    handleCommand(text, fromId);
  }
}

// ─── COMMAND HANDLER ─────────────────────────────────────────────────

void handleCommand(String cmd, String userId) {

  // ── LED1 (USER1 only) ──
  if (cmd == "/led1_on" || cmd == "/led1_off") {
    if (userId != USER1_ID) {
      sendMessage(CHAT_ID, "❌ LAMPU LED1 TIDAK NYALA! INI HAK MILIK USER1!");
      return;
    }
    bool state = (cmd == "/led1_on");
    digitalWrite(LED1_PIN, state ? HIGH : LOW);
    sendMessage(CHAT_ID, state ? "💡 LED1 ON" : "🌑 LED1 OFF");
  }

  // ── LED2 (USER2 only) ──
  else if (cmd == "/led2_on" || cmd == "/led2_off") {
    if (userId != USER2_ID) {
      sendMessage(CHAT_ID, "❌ LAMPU LED2 TIDAK NYALA! INI HAK MILIK USER2!");
      return;
    }
    bool state = (cmd == "/led2_on");
    digitalWrite(LED2_PIN, state ? HIGH : LOW);
    sendMessage(CHAT_ID, state ? "💡 LED2 ON" : "🌑 LED2 OFF");
  }

  // ── LED3 (USER3 only) ──
  else if (cmd == "/led3_on" || cmd == "/led3_off") {
    if (userId != USER3_ID) {
      sendMessage(CHAT_ID, "❌ LAMPU LED3 TIDAK NYALA! INI HAK MILIK USER1!");
      return;
    }
    bool state = (cmd == "/led3_on");
    digitalWrite(LED3_PIN, state ? HIGH : LOW);
    sendMessage(CHAT_ID, state ? "💡 LED3 ON" : "🌑 LED3 OFF");
  }

  // ── LED4 (everyone) ──
  else if (cmd == "/led4_on" || cmd == "/led4_off") {
    bool state = (cmd == "/led4_on");
    digitalWrite(LED4_PIN, state ? HIGH : LOW);
    sendMessage(CHAT_ID, state ? "💡 LED4 ON" : "🌑 LED4 OFF");
  }

  // ── DHT22 ──
  else if (cmd == "/weather" || cmd == "/dht") {
    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    if (isnan(temp) || isnan(hum)) {
      sendMessage(CHAT_ID, "⚠️ DHT NYA CAPEK BANG NGGA BISA KERJA.");
      return;
    }
    String reply = "🌡 Temperature: " + String(temp, 1) + " °C\n";
    reply       += "💧 Humidity: "    + String(hum,  1) + " %";
    sendMessage(CHAT_ID, reply);
  }

  // ── Unknown ──
  else if (cmd.startsWith("/")) {
    sendMessage(CHAT_ID,
      "🤖 Unknown command.\n"
      "Available:\n"
      "/led1_on /led1_off\n"
      "/led2_on /led2_off\n"
      "/led3_on /led3_off\n"
      "/led4_on /led4_off\n"
      "/weather"
    );
  }
}

// ─── GAS MONITOR ─────────────────────────────────────────────────────

void checkGas() {
  int raw = analogRead(MQ2_PIN);

  if (raw > GAS_THRESHOLD && !gasAlertSent) {
    String alert = "🚨 GAS ALERT! Tingkat gas membuat tewas!\n";
    alert += "Sensor value: " + String(raw) + " / 4095\n";
    alert += "Tolong matiin gasnya!";
    sendMessage(CHAT_ID, alert);
    gasAlertSent = true;
  }

  // Reset alert once levels drop back to safe
  if (raw <= GAS_THRESHOLD && gasAlertSent) {
    sendMessage(CHAT_ID, "✅ Udara sudah siap dihirup lagi! (Value: " + String(raw) + ")");
    gasAlertSent = false;
  }
  Serial.println("Gas Value: " + String(raw));
}

// ─── SETUP & LOOP ─────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);

  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
  digitalWrite(LED4_PIN, LOW);

  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  sendMessage(CHAT_ID, "🤖 ESP32-C3 is online and ready!");
}

void loop() {
  getUpdates();

  unsigned long now = millis();
  if (now - lastGasCheck >= GAS_INTERVAL) {
    lastGasCheck = now;
    checkGas();
  }

  delay(500);
}