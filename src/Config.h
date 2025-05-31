#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pin Definitions
namespace PinConfig
{
    static const int CLK = 17; // CLK ENCODER
    static const int DT = 16;  // DT ENCODER
    static const int BUTTON_PIN = 4;
    static const int LED_PIN = 2;
    static const int I2S_DOUT = 25;
    static const int I2S_BCLK = 26;
    static const int I2S_LRC = 27;

        // New: I2C Pins for OLED Display, just temporarily because of defect ESP32
    static const int I2C_SDA = 21;
    static const int I2C_SCL = 23;
}

// Display Configuration
namespace DisplayConfig
{
    static const int SCREEN_WIDTH = 128;
    static const int SCREEN_HEIGHT = 64;
    static const int REFRESH_RATE = 500; // ms
}

// Metronome Settings
struct MetronomeSettings
{
    int bpm;
    const int bpmMax;
    const int bpmMin;
    int lastBpm;
    int pulseWidth;   // LED pulse width
    int ledDelayTime; // LED delay time
    bool isRunning;
    int mode; // 0 = bpm, 1 = sound selection
    float volumeFactor = 100 / 21;
    int triggerDistance; // Calculated from BPM
    int volume = 10;     // 50% of 21

    MetronomeSettings() : bpm(120),
                          bpmMax(240),
                          bpmMin(30),
                          lastBpm(0),
                          pulseWidth(50),
                          ledDelayTime(50),
                          isRunning(true),
                          mode(0),
                          triggerDistance(0)
    {
    }

    void updateBeatInterval()
    {
        // Calculate trigger distance in milliseconds
        // float temp = (1.0f / (bpm / 60.0f)) * 1000.0f;
        float temp = 60000UL / bpm;
        triggerDistance = round(temp);
    }
};

// Timing Variables
struct TimingConfig
{
    uint64_t displayTimestamp;
    uint64_t ledTimestamp;
    uint64_t bpmTimestamp;
    unsigned long lastMillis;
    unsigned long LEDDelayStart;
    unsigned long LEDPulseStart;

    TimingConfig() : displayTimestamp(0),
                     ledTimestamp(0),
                     bpmTimestamp(0),
                     lastMillis(0),
                     LEDDelayStart(0),
                     LEDPulseStart(0)
    {
    }
};

#endif // CONFIG_H
