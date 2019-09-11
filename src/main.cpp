#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>

#include "config.h"

WiFiMulti wiFiMulti;

void startWiFi() {
  WiFi.mode(WIFI_STA);
  wiFiMulti.addAP(ssid, wifipw);

  // wait for WiFi connection
  log_i("Waiting for WiFi to connect...");
  while ((wiFiMulti.run() != WL_CONNECTED)) {
    log_i(".");
  }
  log_i(" connected");
}

void setup() {
  log_i("Hello world: %d", millis());
  log_i("Free PSRAM: %d", ESP.getFreePsram());
  log_i("Free heap: %d", ESP.getFreeHeap());
  log_i("Total PSRAM: %d", ESP.getPsramSize());
  log_i("Total heap: %d", ESP.getHeapSize());

  startWiFi();
}

void loop() {

}