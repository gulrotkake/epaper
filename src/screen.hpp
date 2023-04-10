#include <cstdint>
#include <Arduino.h>

class Screen {
private:
    uint8_t cs_pin, dc_pin, rst_pin, sck_pin, din_pin, busy_pin;
    uint8_t pixel = 0;
    uint8_t count = 0;

    void reset() {
        delay(20);
        digitalWrite(rst_pin, LOW);
        delay(20);
        digitalWrite(rst_pin, HIGH);
        delay(200);
    }

public:
    Screen(
        uint8_t cs_pin_, 
        uint8_t dc_pin_, 
        uint8_t rst_pin_, 
        uint8_t sck_pin_, 
        uint8_t din_pin_, 
        uint8_t busy_pin_) : cs_pin(cs_pin_), dc_pin(dc_pin_), rst_pin(rst_pin_), sck_pin(sck_pin_), din_pin(din_pin_), busy_pin(busy_pin_) {}

    void init() {
        pinMode(cs_pin, OUTPUT);
        pinMode(dc_pin, OUTPUT);
        pinMode(rst_pin, OUTPUT);
        pinMode(sck_pin, OUTPUT);
        pinMode(din_pin, OUTPUT);
        pinMode(busy_pin, INPUT);
        reset();

        // ?
        digitalWrite(cs_pin, HIGH);
        digitalWrite(sck_pin, LOW);

        send(0x01, "\x07\x07\x3f\x3f"); // power reset
        send(0x04, NULL); // power on
        wait();

        send(0x00, "\x0f"); // panel setting KW: 3f, KWR: 2F, BWROTP: 0f, BWOTP: 1f

        send(0x61, "\x03\x20\x01\xe0"); // resolution 0x0320 = 800 x 0x01e0 = 480 (ie 800x480)
        send(0x15, "\x00"); // dual SPI mode
        send(0x50, "\x11\x07"); // vcom/data interval settings
        send(0x60, "\x22"); // tcon setting
    }

    void drawStart() {
        send(0x10, NULL);
    }

    void red() {
        send(0x13, NULL);
    }
    
    void shutdown() {
        send(0x12, NULL);  // display refresh
        delay(100);
        wait();
        send(0x02, NULL);  // POWER_OFF
        wait();
        send(0x07, "\xA5");  // DEEP_SLEEP
    }

    void next(bool setBit) {
        pixel |= setBit? 1 : 0;
        count += 1;
        if (count == 8) {
            digitalWrite(dc_pin, HIGH);
            spi(pixel);
            count = 0;
            pixel = 0;
        } else {
            pixel <<= 1;
        }
    }

    void wait() {
        while(digitalRead(busy_pin) == 0) {
            delay(100);
        }
    }

    void send(char command, const char* args...) {
        digitalWrite(dc_pin, LOW);
        spi(command);
        for (; args && *args != '\0'; ++args) {
            digitalWrite(dc_pin, HIGH);
            spi(*args);
        }
    }

    void spi(char data) {
        digitalWrite(cs_pin, LOW);
        for (int i = 0; i < 8; ++i) {
            if ((data & 0x80) == 0) {
                digitalWrite(din_pin, LOW);
             } else {
                digitalWrite(din_pin, HIGH);
             }
            data <<= 1;
            digitalWrite(sck_pin, HIGH);
            digitalWrite(sck_pin, LOW);
        }
        digitalWrite(cs_pin, HIGH);
    }
};