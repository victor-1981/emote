// ############# LIBRARIES ############### //
// WiFi Library
#include <ESP8266WiFi.h>

// HTTP Request Libraries
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Wire.h>

// Json Library
#include <ArduinoJson.h>

// EmonLib Library for energy monitoring
#include <EmonLib.h>

// Layout Libraries
#include "Icons.h"
#include "Layout.h"

// ############# CONSTANTS ############### //
#define DEBUG_BUTTON_PIN 15 // Define debug button pin
#define SENSOR_PIN A0       // Define sensor pin

// WiFi Network
const char *SSID = "MINASTELECOM_1447"; // WiFi SSID
const char *PASSWORD = "91006531t";     // WiFi Password

// URL Data
String url = "http://morea-ifs.org"; // WebSite URL (using HTTP and not HTTPS)
String deviceId;
String apiToken;
String path;

// Others
const byte interval = 5; // Sample Collection Interval (In Seconds)
int cycles = 60;
bool DEBUG = true; // DEBUG = 1 (Enables the Debug Mode)
byte i = 0;

// Energy Monitor Variables
const int tensao = 110;
double energiaAcumulada = 0;
unsigned long ultimaMedicao = 0;
const double valorKwh = 0.88;

// ############### OBJECTS ################# //
WiFiClient client;
HTTPClient http;
DynamicJsonDocument doc(1024);
Layout layout(128, 64, -1);  // Assuming this library is compatible with ESP8266
Icons icons;                 // Assuming this library is compatible with ESP8266
EnergyMonitor monitor;

// ############# PROTOTYPES ############### //
void initWiFi();
void httpRequest(String path);
String makeRequest(String path);

// ############### SETUP ################# //
void setup() {
  Serial.begin(115200);
  pinMode(DEBUG_BUTTON_PIN, INPUT);

  layout.beginLayout();
  layout.beginTimer();
  layout.drawLogo(icons.sparcLogo());
  layout.drawIcon(0, icons.waterIcon());
  layout.drawIcon(5, icons.loadingIcon());
  layout.drawIcon(6, icons.loadingIcon());
  layout.drawIcon(7, icons.loadingIcon());

  String mac_address = WiFi.macAddress();
  Serial.println("Endereço Mac: " + mac_address);

  initWiFi();

  path = url + "/api/identify-device";
  String data = "macAddress=" + mac_address;

  if (http.begin(client, path)) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(data);
    String payload = http.getString();

    if (httpResponseCode < 0) {
      Serial.println("request error - " + httpResponseCode);
    }

    if (httpResponseCode != HTTP_CODE_OK) {
      Serial.println("Falha no Envio");
      Serial.println(httpResponseCode);
    }

    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.println("Deserialization error");
      return;
    }

    deviceId = doc["id"].as<String>();
    apiToken = doc["api_token"].as<String>();

    Serial.println("Device ID: " + deviceId);
    Serial.println("API Token: " + apiToken);

    http.end();
  }

  Serial.println();
  Serial.print("ESP IP Address: http://" + WiFi.localIP().toString());

  path = url + "/api/get-device-ip";
  data = "deviceId=" + deviceId + "&deviceIp=" + WiFi.localIP().toString() + "&apiToken=" + apiToken;
  Serial.println();

  if (http.begin(client, path)) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(data);
    String payload = http.getString();

    if (httpResponseCode < 0) {
      Serial.println("request error - " + httpResponseCode);
    }

    if (httpResponseCode != HTTP_CODE_OK) {
      Serial.println("Falha no Envio");
      Serial.println(httpResponseCode);
    }

    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.println("Deserialization error");
      layout.drawIcon(6, icons.failedIcon());
      return;
    }

    String responseMessage = doc["message"].as<String>();
    String deviceName = doc["deviceName"].as<String>();
    Serial.println("Response Message: " + responseMessage + " | Device Name: " + deviceName);

    if (deviceName == "null") {
      layout.writeLine(3, "Device: Unnamed");
    } else {
      layout.writeLine(3, "Device: " + deviceName);
    }
    layout.drawIcon(6, icons.keyIcon());

    http.end();
  }

  monitor.current(SENSOR_PIN, 60);
}

// ############### LOOP ################# //
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    layout.drawIcon(7, icons.loadingIcon());
    initWiFi();
  }

  if (digitalRead(DEBUG_BUTTON_PIN) == HIGH && DEBUG == false) {
    DEBUG = true;
    cycles = 3;
    Serial.println("Debug mode ativado");
  } else if (digitalRead(DEBUG_BUTTON_PIN) == HIGH && DEBUG == true) {
    DEBUG = false;
    cycles = 60;
    Serial.println("Debug mode desativado");
  }

  Serial.println(DEBUG);

  unsigned long currentMillis = millis();
  static unsigned long previousMillis = 0;

  if (currentMillis - previousMillis >= interval * 1000) {
    previousMillis = currentMillis;

    double corrente = monitor.calcIrms(1480);
    double potencia = corrente * tensao;

    unsigned long tempoAtual = millis();
    if (ultimaMedicao > 0) {
      double tempoDecorrido = (tempoAtual - ultimaMedicao) / 3600000.0;
      energiaAcumulada += potencia * tempoDecorrido / 1000.0;
    }
    ultimaMedicao = tempoAtual;

    i++;

    layout.writeLine(0, "Corrente: " + String(corrente) + " A");
    layout.writeLine(1, "Potência: " + String(potencia) + " W");
    layout.writeLine(2, "Energia: " + String(energiaAcumulada) + " kWh");

    if (DEBUG) {
      Serial.println("Corrente: " + String(corrente) + " A");
      Serial.println("Potência: " + String(potencia) + " W");
      Serial.println("Energia Acumulada: " + String(energiaAcumulada) + " kWh");
      Serial.println("Número de Coletas Atuais: " + String(i));
    }

    path = url + "/api/get-device-data";
    String data = "deviceId=" + deviceId + "&apiToken=" + apiToken + "&corrente=" + String(corrente) + "&potencia=" + String(potencia) + "&energia=" + String(energiaAcumulada);
    Serial.println();

    if (http.begin(client, path) && i == cycles) {
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      int httpResponseCode = http.POST(data);
      String payload = http.getString();

      if (httpResponseCode < 0) {
        Serial.println("request error - " + httpResponseCode);
        layout.drawIcon(5, icons.failedIcon());
        i = 0;
        previousMillis = 0;
        layout.updateTimer(i, interval);
      }

      if (httpResponseCode != HTTP_CODE_OK) {
        Serial.println("Falha no Envio");
        Serial.println(httpResponseCode);
        layout.drawIcon(5, icons.failedIcon());
        i = 0;
        previousMillis = 0;
        layout.updateTimer(i, interval);
      }

      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.println("Deserialization error");
        layout.drawIcon(5, icons.failedIcon());
        i = 0;
        previousMillis = 0;
        layout.updateTimer(i, interval);
      }

      String responseMessage = doc["message"].as<String>();
      String deviceName = doc["deviceName"].as<String>();
      Serial.println("Response Message: " + responseMessage + " | Device Name: " + deviceName);

      i = 0;
      previousMillis = 0;
      layout.updateTimer(i, interval);
      layout.drawIcon(5, icons.successIcon());

      http.end();
    }
  }
}

// ############### INIT WIFI ################# //
void initWiFi() {
  WiFi.begin(SSID, PASSWORD);

  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(SSID);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  layout.drawIcon(7, icons.wifiIcon());
}

