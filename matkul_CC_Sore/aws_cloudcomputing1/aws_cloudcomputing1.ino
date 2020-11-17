#include "secret.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include "DHT.h"

#define DHTPIN 4
#define DHTTYPE DHT22
#define DOCSIZE 512
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800;
const int   daylightOffset_sec = 0;

DHT dht(DHTPIN, DHTTYPE);


WiFiClientSecure NET = WiFiClientSecure();
MQTTClient IOT = MQTTClient(512);

void cloud_incoming(String &topic, String &payload) {
  Serial.println("cloud_incoming: " + topic + " - " + payload);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.begin(ssid, pass);
  WiFi.setHostname("CC_Sore");
  WiFi.setAutoReconnect(true);

  Serial.println(F("DHTxx Test"));
  dht.begin();

  NET.setCACert(AWS_CERT_CA);
  NET.setCertificate(AWS_CERT_CRT);
  NET.setPrivateKey(AWS_CERT_PRIVATE);

  IOT.begin(endpoint, 8883, NET);
  IOT.onMessage(cloud_incoming);

  int retries = 0;
  while (!IOT.connect("Pras-Things") && retries < 5) {
    Serial.println(".");
    delay(100);
    retries++;
  }

  if (!IOT.connected()) {
    Serial.println("Timeout OI");
    return;
  }

  Serial.println("Connected");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  IOT.subscribe("/topic");
  IOT.subscribe("/sensor/+");
  IOT.subscribe("/sensor/testing");
}

unsigned long TERAKHIR_DIKIRIM = 0;
void loop() {
  delay(2000);
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  // put your main code here, to run repeatedly:
  unsigned long SEKARANG = millis();
  if (SEKARANG - TERAKHIR_DIKIRIM > 10000) {
    StaticJsonDocument<DOCSIZE> doc;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
    }
    String waktu;
    waktu = String(timeinfo.tm_year + 1900) + "-" + String(timeinfo.tm_mon) + "-" + String(timeinfo.tm_mday) + "  "
            + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String (timeinfo.tm_sec);
    Serial.println(waktu);
    Serial.println(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print(F("%  Temperature: "));
    Serial.print(t);
    Serial.print(F("°C "));
    Serial.print(f);
    Serial.print(F("°F  Heat index: "));
    Serial.print(hic);
    Serial.print(F("°C "));
    Serial.print(hif);
    Serial.println(F("°F"));

    doc["id"] = waktu;
    doc["free_heap"] = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    doc["uptime"] = millis() / 1000;
    doc["h"] = h;
    doc["t"] = t;
    doc["f"] = h;

    char jsonBuffer[DOCSIZE];
    serializeJson(doc, jsonBuffer);

    if (!IOT.connected()) {
      int retries = 0;
      while (!IOT.connect("Pras-Things") && retries < 5) {
        Serial.println(".");
        delay(100);
        retries++;
      }
    }
    else
    {
      IOT.publish("/topic/espPras", jsonBuffer );
    }

    TERAKHIR_DIKIRIM = SEKARANG;
  }
  IOT.loop();
}
