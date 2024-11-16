#pragma once
#include <Arduino.h>
#include <cstdint>
#include <memory>

constexpr size_t WIDTH = 800;
constexpr size_t HEIGHT = 480;
constexpr size_t SIZE = (WIDTH / 8) * HEIGHT;

class Screen
{
    uint8_t cs_pin, dc_pin, rst_pin, sck_pin, din_pin, busy_pin;
    uint8_t pixel = 0;
    uint8_t count = 0;
    std::unique_ptr<uint8_t[]> red;
    std::unique_ptr<uint8_t[]> black;

    void reset()
    {
        delay(20);
        digitalWrite(rst_pin, LOW);
        delay(20);
        digitalWrite(rst_pin, HIGH);
        delay(200);
    }

public:
    Screen(uint8_t cs_pin_, uint8_t dc_pin_, uint8_t rst_pin_, uint8_t sck_pin_,
           uint8_t din_pin_, uint8_t busy_pin_)
        : cs_pin(cs_pin_), dc_pin(dc_pin_), rst_pin(rst_pin_), sck_pin(sck_pin_),
          din_pin(din_pin_), busy_pin(busy_pin_), red(new uint8_t[SIZE]),
          black(new uint8_t[SIZE])
    {
        for (size_t i = 0; i < SIZE; i++)
        {
            black[i] = 0xFF;
            red[i] = 0x00;
        }
    }

    ~Screen() {}

    void init()
    {
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

        send(0x06, "\x17\x17\x38\x17");

        send(0x01, "\x07\x07\x3f\x3f"); // power reset
        send(0x04, NULL);               // power on
        wait();

        send(0x00, "\x0f");             // panel setting KW: 3f, KWR: 2F, BWROTP: 0f, BWOTP: 1f
        send(0x61, "\x03\x20\x01\xe0"); // resolution 0x0320 = 800 x 0x01e0 = 480
                                        // (ie 800x480)
        send(0x15, "\x00");             // dual SPI mode
        send(0x50, "\x11\x07");         // vcom/data interval settings
        send(0x60, "\x22");             // tcon setting
    }

    void drawRLE()
    {
        // Read the next color (2bits)
        send(0x10, NULL);
        for (int y = 0; y < 480; ++y)
        {
            for (int x = 0; x < 100; ++x)
            {
                digitalWrite(dc_pin, HIGH);
                spi(black[(y * 100) + x]);
            }
        }
        send(0x92, NULL);

        send(0x13, NULL);
        for (int y = 0; y < 480; ++y)
        {
            for (int x = 0; x < 100; ++x)
            {
                digitalWrite(dc_pin, HIGH);
                spi(red[(y * 100) + x]);
            }
        }
    }

    void bpx(size_t x, size_t y)
    {
        size_t xbyte = (y * 800 + x) / 8;
        size_t xbit = x % 8;
        black[xbyte] &= ~(1 << (7 - xbit));
    }

    void rpx(size_t x, size_t y)
    {
        size_t xbyte = (y * 800 + x) / 8;
        size_t xbit = x % 8;
        red[xbyte] |= (1 << (7 - xbit));
    }

    void shutdown()
    {
        send(0x12, NULL); // display refresh
        delay(100);
        wait();
        send(0x02, NULL); // POWER_OFF
        wait();
        send(0x07, "\xA5"); // DEEP_SLEEP
    }

private:
    void wait()
    {
        while (digitalRead(busy_pin) == 0)
        {
            delay(100);
        }
    }

    void send(char command, const char *args...)
    {
        digitalWrite(dc_pin, LOW);
        spi(command);
        for (; args && *args != '\0'; ++args)
        {
            digitalWrite(dc_pin, HIGH);
            spi(*args);
        }
    }

    void spi(char data)
    {
        digitalWrite(cs_pin, LOW);
        for (int i = 0; i < 8; ++i)
        {
            if ((data & 0x80) == 0)
            {
                digitalWrite(din_pin, LOW);
            }
            else
            {
                digitalWrite(din_pin, HIGH);
            }
            data <<= 1;
            digitalWrite(sck_pin, HIGH);
            digitalWrite(sck_pin, LOW);
        }
        digitalWrite(cs_pin, HIGH);
    }
};
