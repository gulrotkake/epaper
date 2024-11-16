#include <Arduino.h>

#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <Update.h>

#include "http.hpp"
#include "screen.hpp"

constexpr int VERSION = 1;

const char* ssid = SSID_HERE;
const char* password = WIFI_HERE;

const char* server = HOSTNAME_HERE;

namespace pins
{
    constexpr uint8_t CS = 2;
    constexpr uint8_t DC = 27;
    constexpr uint8_t RST = 13;
    constexpr uint8_t BUSY = 5;
    constexpr uint8_t SCK = 9;
    constexpr uint8_t DIN = 10;
} // namespace pins

void deepSleep(unsigned long stime)
{
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_sleep_enable_timer_wakeup(stime * 60UL * 1000000UL);
    Serial.flush();
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    esp_deep_sleep_start();
}

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    delay(1000);

    // Wait for connection
    int count = 0;
    while (WiFi.status() != WL_CONNECTED && count < 20)
    {
        delay(500);
        count += 1;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Failed to connect to WiFi, giving up.");
    }

    Screen screen(pins::CS, pins::DC, pins::RST, pins::SCK, pins::DIN,
        pins::BUSY);

    uint32_t bytesRead = 0;

    auto onData = [&screen, &bytesRead](const char* data, size_t count)
        {
            for (size_t i = 0; i < count; ++i, ++bytesRead)
            {
                if (data[i] == 'r')
                {
                    screen.rpx(bytesRead % WIDTH, bytesRead / WIDTH);
                }
                else if (data[i] == 'b')
                {
                    screen.bpx(bytesRead % WIDTH, bytesRead / WIDTH);
                }
            }
        };

    auto onComplete = [&screen](const char* errorMsg)
        {
            if (!errorMsg)
            {
                screen.init();
                screen.drawRLE();
                screen.shutdown();
            }
            else
            {
                Serial.printf("HTTP failure: %s\n", errorMsg);
            }
        };

    auto firmwareUpdate = [](HTTPClient& client, const char* errMsg)
        {
            if (errMsg) {
                Serial.printf("Ignoring firmware update: %s\n", errMsg);
                return false;
            }

            // Get firmware version header
            auto version = client.header("x-firmware");
            // Not a firmware response
            if (version.isEmpty()) {
                return false;
            }

            int v;
            try {
                v = std::stoi(version.c_str());
            }
            catch (const std::invalid_argument& e) {
                Serial.printf("Bad version string: %s\n", version);
                return false;
            }
            catch (const std::out_of_range& e) {
                Serial.printf("Out of range version string: %s\n", version);
                return false;
            }

            if (v <= VERSION) {
                return false;
            }

            int size = client.getSize();
            Stream* stream = client.getStreamPtr();

            Serial.printf("Starting firmware update, size: %d\n", size);
            if (!Update.begin(size)) {
                Serial.println("Not enough space to begin OTA");
                return false;
            }

            size_t written = Update.writeStream(*stream);
            if (written != size) {
                Serial.println("Download failure, ignoring update");
                return false;
            }

            if (!Update.end()) {
                Serial.printf("Error: %s\n" + Update.getError());
                return false;
            }

            if (!Update.isFinished()) {
                Serial.println("Update failed");
                return false;
            }
            return true;
        };

    if (GETStream((std::string(server) + "/firmware").c_str(), firmwareUpdate)) {
        Serial.println("Firmware updated, rebooting");
        ESP.restart();
    }
    else
    {
        GET(server, onData, onComplete);
        Serial.println("All done, powering down for 60 minutes.");
        deepSleep(60);
    }
}

void loop() {}
