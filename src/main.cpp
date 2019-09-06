#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config.h"

// Name derived from external file name
// See: https://docs.platformio.org/en/latest/platforms/espressif32.html#embedding-binary-data
extern const uint8_t rootCACertificate[] asm("_binary_src_rootCA_pem_start");

WiFiMulti wiFiMulti;

void startWiFi() {
  WiFi.mode(WIFI_STA);
  wiFiMulti.addAP(ssid, wifipw);

  // wait for WiFi connection
  log_v("Waiting for WiFi to connect...");
  while ((wiFiMulti.run() != WL_CONNECTED)) {
    log_v(".");
  }
  log_v(" connected");
}

void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  log_v("Waiting for NTP time sync: ");
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    log_v(".");
    yield();
    nowSecs = time(nullptr);
  }

  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  log_v("Current time: %s", asctime(&timeinfo));
}

void sendPush() {
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert((const char *)rootCACertificate);

    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      log_v("[HTTPS] begin...\n");

      if (https.begin(*client, "https://api.pushbullet.com/v2/pushes")) {  // HTTPS
        log_v("[HTTPS] POST...");
        // start connection and send HTTP header
        https.addHeader("Access-Token", apikey);
        https.addHeader("Content-Type",  "application/json");
        int httpCode = https.POST("{\"body\":\"My first push\",\"title\":\"Hello World\",\"type\":\"note\"}");
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          log_v("[HTTPS] POST... code: %d", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            log_v("Response: %s", https.getString().c_str());
          }
        } else {
          log_e("[HTTPS] POST... failed, error: %s", https.errorToString(httpCode).c_str());
        }
  
        https.end();
      } else {
        log_e("[HTTPS] Unable to connect");
      }

      // End extra scoping block
    }
  
    delete client;
  } else {
    log_v("Unable to create client");
  }

}

void setup() {
  Serial.begin(115200);
  log_v("Hello world: %d", millis());
  log_d("Free PSRAM: %d", ESP.getFreePsram());
  log_i("Free heap: %d", ESP.getFreeHeap());
  log_w("Total PSRAM: %d", ESP.getPsramSize());
  log_e("Total heap: %d", ESP.getHeapSize());

  startWiFi();

  setClock();

  sendPush();
}

void loop() {

}