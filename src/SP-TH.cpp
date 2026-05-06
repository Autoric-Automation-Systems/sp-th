/*  Smart Plant APP
    https://smartplantapp.app.br/

    Module:
    SP-TH | Smart Plant Temperature Humidity
    ESP32-C3 DevKitM-1
*/

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_AHTX0.h>
#include <esp_sleep.h>
#include <time.h>

// ================= CONFIG ==============
#define VERSION "1.0"
#define DEVICE_TYPE "SP-TH"
String nameAP = "Smart Plant APP - " + String(DEVICE_TYPE);

//========================================

// RTC_DATA_ATTR persiste automaticamente entre deep sleeps sem necessidade de CRC
RTC_DATA_ATTR unsigned long heartbeatInterval = 10000;

// ********* Local Test ****************
bool isLocal = false;

String baseUrl = isLocal
                     ? "http://192.168.100.103:3000"
                     : "https://smartplant.app.br";

String apiHeartbeat = baseUrl + "/api/devices/heartbeat";
String apiEvents = baseUrl + "/api/devices/events";
//**************************************

//================ NTP =======================
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

//================ Battery =================
#define BATTERY_PIN 1 // GPIO conectado ao ponto médio do divisor 10K/10K

// ================= GLOBAL =================
Adafruit_AHTX0 aht;

unsigned long lastHeartbeat = 0;

float lastTemperature = 0.0;
int lastBattery = 0;
int lastWifi = -100;

String mac;

// ================= EVENT =================
struct Event
{
  String name;
  float value;
  unsigned long ts; // epoch
};

Event buffer[100];
int bufferIndex = 0;

// ================= NTP =================
void setupTime()
{

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  Serial.print("Sincronizando NTP");

  time_t now = time(nullptr);

  int retry = 0;

  while (now < 100000 && retry < 20)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    retry++;
  }

  Serial.println();

  if (now < 100000)
  {
    Serial.println("Falha NTP");
  }
  else
  {
    Serial.println("NTP OK");
    Serial.println(ctime(&now));
  }
}

// ================= ADD EVENT =================
void addEvent(String name, float value)
{

  if (bufferIndex >= 100)
  {
    Serial.println("BUFFER CHEIO");
    return;
  }

  float rounded = round(value * 100.0) / 100.0;

  time_t now = time(nullptr);

  if (now < 100000)
  {
    Serial.println("Sem tempo válido ainda");
    return;
  }

  buffer[bufferIndex].name = name;
  buffer[bufferIndex].value = rounded;
  buffer[bufferIndex].ts = now;

  Serial.print("Evento: ");
  Serial.print(name);
  Serial.print(" = ");
  Serial.println(rounded);

  bufferIndex++;
}

// ================= SEND EVENTS =================
void sendEvents()
{
  if (WiFi.status() != WL_CONNECTED)
    return;
  if (bufferIndex == 0)
    return;
  Serial.println("Enviando eventos " + apiEvents);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, apiEvents);
  http.addHeader("Content-Type", "application/json");
  DynamicJsonDocument doc(2048);
  doc["mac"] = mac;
  JsonArray events = doc.createNestedArray("events");
  for (int i = 0; i < bufferIndex; i++)
  {
    JsonObject ev = events.createNestedObject();
    ev["name"] = buffer[i].name;
    ev["value"] = buffer[i].value;
    ev["ts"] = buffer[i].ts;
  }
  String body;
  serializeJson(doc, body);
  Serial.println(body);
  int httpCode = http.POST(body);
  Serial.print("Events HTTP: ");
  Serial.println(httpCode);
  if (httpCode > 0)
  {
    Serial.println(http.getString());
    Serial.println();
  }
  http.end();
  bufferIndex = 0;
}

// ================= HEARTBEAT =================
void sendHeartbeat()
{
  if (WiFi.status() != WL_CONNECTED)
    return;
  Serial.println("Enviando heartbeat " + apiHeartbeat);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, apiHeartbeat);
  http.addHeader("Content-Type", "application/json");
  DynamicJsonDocument doc(256);
  doc["mac"] = mac;
  doc["type"] = DEVICE_TYPE;
  doc["version"] = VERSION;
  doc["wifi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  doc["uptime"] = millis();
  String body;
  serializeJson(doc, body);
  Serial.println(body);
  int httpCode = http.POST(body);
  Serial.print("Heartbeat HTTP: ");
  Serial.println(httpCode);
  if (httpCode > 0)
  {
    String payload = http.getString();
    Serial.println(payload);
    DynamicJsonDocument resp(256);
    deserializeJson(resp, payload);
    if (resp["heartbeatInterval"])
    {
      unsigned long newValue = resp["heartbeatInterval"];
      if (newValue != heartbeatInterval)
      {
        heartbeatInterval = newValue; // RTC_DATA_ATTR persiste automaticamente
        Serial.print("Novo intervalo salvo: ");
        Serial.println(heartbeatInterval);
        Serial.println();
      }
      else
      {
        Serial.println("Não foi necessário atualizar heartbeatInterval");
        Serial.println();
      }
    }
  }
  http.end();
}

// ================= BATTERY =================
int getBatteryPercent(float voltage)
{
  float volt_min = 3.2;
  float volt_max = 4.0;
  if (voltage >= volt_max)
    return 100;
  if (voltage <= volt_min)
    return 0;
  return (int)((voltage - volt_min) * 100.0 / (volt_max - volt_min));
}

float calibration = -0.97;

//================== CheckVoltage ================
void checkVoltage()
{
  // Divisor 10K/10K: Vpin = Vbat/2 → Vbat = Vpin * 2
  float voltage = (analogReadMilliVolts(BATTERY_PIN) / 1000.0) * 2.0 + calibration;
  printf("Bateria: %.2f V\n", voltage);
  int percent = getBatteryPercent(voltage);
  if (abs(percent - lastBattery) > 5)
  {
    addEvent("battery", percent);
    lastBattery = percent;
  }
}

// ================= TEMP =================
void checkTemperature()
{
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  float t = temp.temperature;
  float h = humidity.relative_humidity;

  if (abs(t - lastTemperature) > 0.2)
  {

    addEvent("temperature", t);
    addEvent("humidity", h);

    lastTemperature = t;
  }
}

// =================Check  WIFI =================
void checkWifi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi não conectado");
    return;
  }
  int wifi = WiFi.RSSI();
  if (wifi > 0 || wifi < -100)
  {
    Serial.print("RSSI inválido: ");
    Serial.println(wifi);
    return;
  }
  if (abs(wifi - lastWifi) > 10)
  {
    addEvent("wifi", wifi);
    lastWifi = wifi;
  }
}
//=========== Conect WIFI =======================
void connectWifi()
{
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Tentando WiFi...");
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 40)
    {
      delay(500);
      Serial.print(".");
      retry++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("WiFi conectado");
      return;
    }

    Serial.println("Falha WiFi, abrindo AP por 2 min...");
    WiFiManager wm;
    wm.setConfigPortalTimeout(120);
    wm.startConfigPortal(nameAP.c_str());
    WiFi.mode(WIFI_STA); // garante modo STA após fechar o portal
    Serial.println("AP encerrado, tentando de novo...");
  }
}
//======== Disconect WIFI =======================
void disconnectWifi()
{
  Serial.println("Desligando WiFi...");
  WiFi.mode(WIFI_OFF);
  int retry = 0;
  while (WiFi.status() == WL_CONNECTED && retry < 20)
  {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi ainda conectado");
  }
  else
  {
    Serial.println("WiFi Desligado");
  }
}

// ================= LED =================
unsigned long lastBlink = 0;
void blinkLed()
{
  if (millis() - lastBlink > 150)
  {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    lastBlink = millis();
  }
}

// ================= AHT =================
void startAHT()
{
  // ESP32-C3: use GPIOs disponíveis para I2C. Ajuste conforme seu hardware.
  Wire.begin(8, 9);      // SDA=GPIO8, SCL=GPIO9
  Wire.setClock(100000); // estabilidade > velocidade
  Serial.print("Iniciando AHT...");
  bool ok = false;
  for (int i = 0; i < 10; i++)
  {
    if (aht.begin())
    {
      ok = true;
      break;
    }
    Serial.print(".");
    delay(300);
  }
  if (!ok)
  {
    Serial.print("Erro AHT10");
    Serial.println();
    Serial.println();
    addEvent("aht_error", 1);
    return; // segue o fluxo sem sensor
  }
  Serial.print("AHT OK");
  Serial.println();
  Serial.println();
}

//================= Start Message ================
void startmsg()
{
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000)
  {
  }
  Serial.println();
  Serial.println();
  Serial.print("Boot Smart Plant Version: ");
  Serial.print(VERSION);
  Serial.print(" Type: ");
  Serial.println(DEVICE_TYPE);
  Serial.println("Acordando do Deep Sleep!");
  Serial.println();
}

// ================= SETUP =================
void setup()
{
  startmsg();

  connectWifi();

  Serial.println("WiFi conectado");

  mac = WiFi.macAddress();
  Serial.print("MAC: ");
  Serial.println(mac);
  Serial.println();

  setupTime();

  startAHT();

  checkTemperature();
  checkVoltage();
  checkWifi();

  sendHeartbeat();
  sendEvents();

  WiFi.mode(WIFI_OFF);

  Serial.println("Entrando em Deep Sleep...");

  // ESP32: configura wakeup por timer (microssegundos) e entra em deep sleep
  esp_sleep_enable_timer_wakeup((uint64_t)heartbeatInterval * 1000ULL);
  esp_deep_sleep_start();
}

// ================= LOOP =================
void loop()
{
  // checkVoltage();
  // delay(1000);
}