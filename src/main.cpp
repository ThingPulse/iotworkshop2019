#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "camera.h"

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

void postFile(FileUpload* fileUpload, camera_fb_t * fb) {
  String url = fileUpload->uploadUrl;
  /**************************************************************************** 
   * 
   * Start URL parsing
   * 
   */
  int index = url.indexOf(':');
  if(index < 0) {
      log_e("failed to parse protocol");
      return;
  }

  url.remove(0, (index + 3)); // remove http:// or https://

  index = url.indexOf('/');
  String host = url.substring(0, index);
  url.remove(0, index); // remove host part

  // get Authorization
  index = host.indexOf('@');
  if(index >= 0) {
      // auth info
      String auth = host.substring(0, index);
      host.remove(0, index + 1); // remove auth part including @
  }
  String post_host;
  // get port
  index = host.indexOf(':');
  if(index >= 0) {
      post_host = host.substring(0, index); // hostname
      host.remove(0, (index + 1)); // remove hostname + :
      post_host = host.toInt(); // get port
  } else {
      post_host = host;
  }
  String path = url;
  /** END URL parsing ********************************************************/

  log_i("------ Starting file upload -------");
  WiFiClientSecure *client = new WiFiClientSecure;
  String fileName = "cat.jpg";
  uint32_t fileSize = fb->len;
  uint16_t post_port = 443;

  // print content length and host
  log_i("content length: %d", fileSize);
  log_i("connecting to '%s'", post_host.c_str());

  // try connect or return on fail
  if (!client->connect(post_host.c_str(), post_port)) {
    log_e("HTTP connection failure");
    return;
  }

  // Create a URI for the request
  log_i("Connected to server");
  log_i("Requesting URL: '%s'", url.c_str());

  // Make a HTTP request and add HTTP headers
  String boundary = "AX0011";
  String contentType = "image/jpeg";
  String portString = String(post_port);
  String hostString = String(post_host);

  // POST headers
  String postHeader = "POST " + path + " HTTP/1.1\r\n";
  postHeader += "Host: " + hostString + "\r\n";
  postHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  postHeader += "User-Agent: ESP32/mailbox-cam\r\n";
  postHeader += "Keep-Alive: 300\r\n";
  postHeader += "Connection: keep-alive\r\n";

  // request head
  String requestHead = "--" + boundary + "\r\n";
  requestHead += "Content-Disposition: form-data; name=\"file\"; filename=\"" + fileName + "\"\r\n";
  requestHead += "Content-Type: " + contentType + "\r\n\r\n";

  // request tail
  String tail = "\r\n\r\n--" + boundary + "--\r\n\r\n";

  // content length
  int contentLength = requestHead.length() + fileSize + tail.length();
  postHeader += "Content-Length: " + String(contentLength, DEC) + "\r\n\r\n";

  /* START communication */
  log_i("%s", postHeader.c_str());
  client->print(postHeader.c_str());

  log_i("%s", requestHead.c_str());
  client->print(requestHead.c_str());

  size_t sent = 0;
  size_t remaining = fileSize;
  size_t offset = 0;
  while (true) {
    sent = client->write(fb->buf + offset, remaining);
    remaining = remaining - sent;
    offset += sent;
    log_i("Sent: %d, remaining: %d, offset: %d", sent, remaining, offset);
    if (remaining <= 0 || sent <= 0) {
      break;
    }
  }

  log_i("%s", tail.c_str());
  client->print(tail.c_str());

  log_i("request sent, waiting for response");

  while (client->connected()) {
    String line = client->readStringUntil('\n');
    log_i("%s", line.c_str());
    if (line == "\r") {
      log_i("done receiving headers");
      break;
    }
  }

  String line = client->readStringUntil('\n');

  log_i("response body");
  log_i("==========");
  log_i("%s", line.c_str());
  log_i("==========");
  log_i("closing connection");  

  delete client;
}


void setup() {
  Serial.begin(115200);
  log_i("Hello world: %d", millis());
  log_i("Free PSRAM: %d", ESP.getFreePsram());
  log_i("Free heap: %d", ESP.getFreeHeap());
  log_i("Total PSRAM: %d", ESP.getPsramSize());
  log_i("Total heap: %d", ESP.getHeapSize());

  // Init with config
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
      log_e("Camera init failed with error 0x%x", err);
      return;
  }

  // Acquire Image
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
      log_e("Camera capture failed");
      return;
  }

  startWiFi();

  setClock();

  FileUpload fileUpload;

  sendUploadRequest(&fileUpload);
  if (fileUpload.status >=200 && fileUpload.status < 400) {
    postFile(&fileUpload, fb);
    sendPush(&fileUpload);
  }

  // esp_sleep_enable_ext0_wakeup(GPIO_NUM_39,1);
  // log_i("Going to sleep");
  // esp_deep_sleep_start();
}

void loop() {

}