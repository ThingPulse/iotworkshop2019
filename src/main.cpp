#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include "config.h"

// Name derived from external file name
// See: https://docs.platformio.org/en/latest/platforms/espressif32.html#embedding-binary-data
extern const uint8_t rootCACertificate[] asm("_binary_src_rootCA_pem_start");
extern const uint8_t cat_start[] asm("_binary_src_cat_jpg_start");
extern const uint8_t cat_end[] asm("_binary_src_cat_jpg_end");

typedef struct FileUpload {
  uint8_t status;
  String uploadUrl;
  String fileUrl;
} FileUpload;

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

void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  log_i("Waiting for NTP time sync: ");
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    log_i(".");
    yield();
    nowSecs = time(nullptr);
  }

  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  log_i("Current time: %s", asctime(&timeinfo));
}

void sendPush(FileUpload *fileUpload) {
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert((const char *)rootCACertificate);

    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      log_i("[HTTPS] begin...\n");

      if (https.begin(*client, "https://api.pushbullet.com/v2/pushes")) {  // HTTPS
        log_i("[HTTPS] POST...");
        // start connection and send HTTP header
        https.addHeader("Access-Token", apikey);
        https.addHeader("Content-Type",  "application/json");
        String message = "{\"body\":\"My first push\",\"title\":\"Hello World\",\"file_name\": \"cat.jpg\",\"type\":\"file\", \"file_url\": \"" + fileUpload->fileUrl + "\"}";
        log_i("Body: %s", message.c_str());
        int httpCode = https.POST(message);
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          log_i("[HTTPS] POST... code: %d", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            log_i("Response: %s", https.getString().c_str());
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
    log_i("Unable to create client");
  }

}

void sendUploadRequest(FileUpload *fileUpload) {
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert((const char *)rootCACertificate);

    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      log_i("[HTTPS] begin...\n");
 
      if (https.begin(*client, "https://api.pushbullet.com/v2/upload-request")) {  // HTTPS
        log_i("[HTTPS] POST...\n");
        // start connection and send HTTP header
        https.addHeader("Access-Token", apikey);
        https.addHeader("Content-Type", "application/json");
        int httpCode = https.POST("{\"file_name\":\"cat.jpg\",\"file_type\":\"image/jpeg\"}");
  
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          log_i("[HTTPS] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();

            DynamicJsonDocument doc(1024);
            DeserializationError err = deserializeJson(doc, payload);
            JsonObject obj = doc.as<JsonObject>();
            if (err) {
              log_e("deserializeJson() failed with code: %s ", err.c_str());
              return;
            }
            fileUpload->status = httpCode;
            fileUpload->uploadUrl = obj["upload_url"].as<String>();
            fileUpload->fileUrl = doc["file_url"].as<String>();
            log_i("Upload url: %s, fileUrl: %s, ", fileUpload->uploadUrl.c_str(), fileUpload->fileUrl.c_str());

            return;

          }
        } else {
          fileUpload->status = httpCode;
          log_e("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
  
        https.end();
      } else {
        log_e("[HTTPS] Unable to connect\n");
      }

      // End extra scoping block
    }
  
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
  fileUpload->status = -1;
  return;

}

void postFile(FileUpload* fileUpload) {
  log_i("------ Starting file upload -------");
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert((const char *)rootCACertificate);

    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
      https.setReuse(true);
      Serial.print("[HTTPS] begin...\n");

 
      if (https.begin(*client, fileUpload->uploadUrl)) {  // HTTPS
        String boundary = "AX0011";
        int fileSize = cat_end - cat_start;
        const uint8_t* filePayload = cat_start;
        String contentType = "image/jpeg";

        Serial.print("[HTTPS] POST...\n");
        // start connection and send HTTP header
        https.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

        // request header
        String requestHead = "--" + boundary + "\r\n";
        requestHead += "Content-Disposition: form-data; name=\"file\"; filename=\"cat.jpg\"\r\n";
        requestHead += "Content-Type: " + contentType + "\r\n\r\n";

        // request tail
        String requestTail = "\r\n\r\n--" + boundary + "--\r\n\r\n";

        int contentLength = requestHead.length() + fileSize + requestTail.length();

        https.addHeader("Content-Length", String(contentLength));
        log_i("Content-Length: %d", contentLength);

        uint8_t *payload = (uint8_t*) malloc(contentLength);
        memcpy(payload, requestHead.c_str(), requestHead.length());
        memcpy(payload + requestHead.length(), filePayload, fileSize);
        memcpy(payload + requestHead.length() + fileSize, requestTail.c_str(), requestTail.length());

        int httpCode = https.POST(payload, contentLength);
  
        free(payload);
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            Serial.println(https.getString());
            

          }
        } else {

          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
  
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }

      // End extra scoping block
    }
  
    delete client;

  } else {
    Serial.println("Unable to create client");
  }

}


void setup() {
  Serial.begin(115200);
  log_i("Hello world: %d", millis());
  log_i("Free PSRAM: %d", ESP.getFreePsram());
  log_i("Free heap: %d", ESP.getFreeHeap());
  log_i("Total PSRAM: %d", ESP.getPsramSize());
  log_i("Total heap: %d", ESP.getHeapSize());

  startWiFi();

  setClock();

  FileUpload fileUpload;

  sendUploadRequest(&fileUpload);
  if (fileUpload.status >=200 && fileUpload.status < 400) {
    postFile(&fileUpload);
    sendPush(&fileUpload);
  }
}

void loop() {

}