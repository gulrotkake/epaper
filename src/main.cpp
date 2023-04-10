#include <esp_bt.h>
#include <HTTPClient.h>

#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "screen.hpp"

const char* ssid = SSID_HERE;
const char* password = PASSWORD_HERE;

#define CS 2
#define DC 27
#define RST 13
#define BUSY 5
#define SCK 9
#define DIN 10

void deepSleep(unsigned long stime) {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();
  esp_bt_controller_disable();
  esp_sleep_enable_timer_wakeup(stime * 60UL * 1000000UL);
  Serial.flush();
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_deep_sleep_start();
}

Screen scr(CS, DC, RST, SCK, DIN, BUSY);

bool httpGETRequest(const char* uri) {
  HTTPClient http;
  char buffer[512];
  uint32_t bytesRead = 0;
  uint16_t color = 0x0000;
  if (http.begin(uri) > 0) {
    int resCode = http.GET();
    if (resCode > 0) {
      if (resCode == HTTP_CODE_OK) {
        int size = http.getSize();
        WiFiClient* stream = http.getStreamPtr();
        scr.drawStart();
        do {
          if (!http.connected()) {
            Serial.println("Connection lost");
            http.end();
            goto error;
          }

          size_t count = stream->readBytes(buffer, sizeof(buffer));
          for (uint32_t i = 0; i<count; i++, bytesRead++) {
            if (bytesRead == 384000) {
              color = 0xF800;
              scr.red();
            }
            scr.next((color == 0xF800 && buffer[i] == 'r' || color == 0x0000 && buffer[i] == 'w'));
          }
        } while (stream->available() > 0);
        Serial.printf("Read %d bytes, content-length %d\n", bytesRead, size);
      } else {
        Serial.printf("[HTTP] Unexpected return code: %d\n", resCode);
        goto error;
      }
    } else {
      Serial.printf("[HTTP] GET failed, error: %s (bytes read: %d)\n", http.errorToString(resCode).c_str(), bytesRead);
      goto error;
    }
  } else {
    Serial.print("Error code: ");
    Serial.println("Unable to connect");
    goto error;
  }

  return true;
error:
  http.end();
  return false;
}

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  Serial.println("Starting wifi!");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  int count = 0;
  while (WiFi.status() != WL_CONNECTED && count < 20) {
    delay(500);
    count += 1;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi, giving up.");
  }

  scr.init();
  httpGETRequest("http://192.168.1.42:8080/");
  scr.shutdown();

  Serial.println("All done, powering down for 60 minutes.");
  deepSleep(60);
}

void loop() {
  delay(5000);
}
