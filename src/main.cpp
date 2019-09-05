#include <Arduino.h>

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>



void startWiFi() {
  AsyncWebServer server(80);
  DNSServer dns;

  AsyncWiFiManager wifiManager(&server,&dns);
  wifiManager.autoConnect("AutoConnectAP");

  log_v("Connected to WiFi");
}

void setup() {
  log_v("Hello world: %d", millis());
  log_d("Free PSRAM: %d", ESP.getFreePsram());
  log_i("Free heap: %d", ESP.getFreeHeap());
  log_w("Total PSRAM: %d", ESP.getPsramSize());
  log_e("Total heap: %d", ESP.getHeapSize());

  startWiFi();
}

void loop() {

}