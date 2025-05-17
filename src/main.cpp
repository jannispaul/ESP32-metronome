#include <Arduino.h>
#include <ESP32Encoder.h>
#include "Button2.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2lib.h>
#include "Audio.h"
#include "FS.h"
#include "SPIFFS.h"
#include "soundFileLoader.h"
#include "bitmaps.h"
#include "Config.h"

// === Config Structures ===
MetronomeSettings metronomeSettings;
TimingConfig timingConfig;

// === Hardware Interfaces ===
ESP32Encoder encoder;
Button2 button;
Audio audio;

// === Display ===
#define OLED_RESET LED_BUILTIN
Adafruit_SSD1306 display(DisplayConfig::SCREEN_WIDTH, DisplayConfig::SCREEN_HEIGHT, &Wire, OLED_RESET);

// === Sound ===
const char *soundFiles[MAX_SOUND_FILES];
int soundFileCount;
int soundIndex = 0;

// === State Flags ===
bool LEDDelayActive = false;
bool metronomRunning = true;
int mode = 0;

// Button handlers
void click(Button2 &btn);
void released(Button2 &btn);
void longClickDetected(Button2 &btn);
void longClick(Button2 &btn);
void doubleClick(Button2 &btn);
void tripleClick(Button2 &btn);

// Core handlers
void handleEncoder();
void updateBPM();
void updateVolume();
void selectSound();
void updateMode();
void updateUI();
void handleMode();

// Metronome logic
bool shouldTriggerMetronome();
void triggerMetronome();
bool shouldPulseLED();
void pulseLED();
bool shouldTurnOffLED();
void LEDDelay();
void audioClick(int soundIndex);


void setup() {
    u8g2.begin();
    Serial.begin(115200);

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
        return;
    }

    File root = SPIFFS.open("/");
    while (File file = root.openNextFile()) {
        Serial.print("FILE: "); Serial.println(file.name());
    }

    soundFileCount = loadSoundFiles(soundFiles, MAX_SOUND_FILES);
    Serial.printf("%d sound files loaded.\n", soundFileCount);

    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(PinConfig::DT, PinConfig::CLK);

    button.begin(PinConfig::BUTTON_PIN);
    button.setLongClickTime(1000);
    button.setClickHandler(click);
    button.setReleasedHandler(released);
    button.setLongClickDetectedHandler(longClickDetected);
    button.setLongClickHandler(longClick);
    button.setDoubleClickHandler(doubleClick);
    button.setTripleClickHandler(tripleClick);

    pinMode(PinConfig::LED_PIN, OUTPUT);
    digitalWrite(PinConfig::LED_PIN, LOW);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        while (true); // halt
    }

    audio.setPinout(PinConfig::I2S_BCLK, PinConfig::I2S_LRC, PinConfig::I2S_DOUT);
    audio.setVolume(metronomeSettings.volume);

    encoder.setCount(metronomeSettings.bpm * 2);
}
void loop() {
    handleMode();
    button.loop();

    if (shouldTriggerMetronome()) triggerMetronome();
    if (shouldPulseLED()) {
        pulseLED();
        LEDDelayActive = false;
    }
    if (shouldTurnOffLED()) digitalWrite(PinConfig::LED_PIN, LOW);

    audio.loop();
}

bool shouldTriggerMetronome() {
    return millis() - timingConfig.bpmTimestamp > metronomeSettings.triggerDistance;
}

void triggerMetronome() {
    if (!metronomRunning) return;
    audio.stopSong(); // stop any previous playbacks (optional)
    delay(5);          // ensure I2S is released
    audioClick(soundIndex);
    pulseLED();
    LEDDelay();
    timingConfig.bpmTimestamp = millis();
}

void audioClick(int soundIndex) {
    if (soundIndex >= 0 && soundIndex < soundFileCount) {
        audio.connecttoFS(SPIFFS, soundFiles[soundIndex]);
    }
}
void pulseLED() {
    timingConfig.ledTimestamp = millis();
    digitalWrite(PinConfig::LED_PIN, HIGH);
}

bool shouldPulseLED() {
    return LEDDelayActive && millis() - timingConfig.LEDDelayStart > metronomeSettings.ledDelayTime;
}

bool shouldTurnOffLED() {
    return millis() - timingConfig.ledTimestamp > metronomeSettings.pulseWidth;
}

void LEDDelay() {
    timingConfig.LEDDelayStart = millis();
    LEDDelayActive = true;
}

void handleMode() {
    handleEncoder();
    updateUI();
}

void updateUI()
{
    // Setup display
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    // Draw ui based on mode
    if (mode == 0)
    {
        // Set bpm as string
        u8g2.setFont(u8g_font_profont29);
        String bpmString = String(metronomeSettings.bpm);
        u8g2.drawStr(57, 41, bpmString.c_str()); // Draw tempo on the display
    }
    else if (mode == 1)
    {
        u8g2.setFont(u8g_font_profont29);
        String soundString = "S" + String(soundIndex);
        u8g2.drawStr(57, 41, soundString.c_str()); // Draw tempo on the display

        // u8g2.drawXBM(0, 48, 16, 16, image_Property_1_Battery_75_bits);
        u8g2.setFont(u8g_font_5x7); // Change this to the correct font name
        u8g2.drawStr(36, 59, "Select a sound");
    }
    else if (mode == 2)
    {
        u8g2.setFont(u8g_font_profont29);

        String volumeString = String((audio.getVolume() * 100) / 21) + "%";
        u8g2.drawStr(57, 41, volumeString.c_str()); // Draw volume on the display
        u8g2.setFont(u8g_font_5x7);                 // Change this to the correct font name
        u8g2.drawStr(36, 59, "Volume");
    }
    u8g2.drawXBM(0, 48, 16, 16, image_Property_1_Battery_100_bits);
    u8g2.drawXBM(0, 24, 16, 16, image_Property_1_Volume_bits);
    u8g2.setFont(u8g_font_4x6); // Change this to the correct font name
    u8g2.drawStr(2, 7, "4");
    u8g2.drawStr(10, 14, "4");
    u8g2.drawLine(5, 10, 10, 5);
    // u8g2.setFont(u8g_font_5x7); // Change this to the correct font name
    // u8g2.drawStr(36, 59, "+1 EVERY 32 BARS");

    // Update the display
    u8g2.sendBuffer();
}
void handleEncoder() {
    switch (mode) {
        case 0: updateBPM(); break;
        case 1: selectSound(); break;
        case 2: updateVolume(); break;
    }
}

void updateBPM() {
    int bpm = encoder.getCount() / 2;
    bpm = constrain(bpm, metronomeSettings.bpmMin, metronomeSettings.bpmMax);
    encoder.setCount(bpm * 2);
    metronomeSettings.bpm = bpm;
    metronomeSettings.updateTriggerDistance();
}

void selectSound() {
    int index = encoder.getCount() / 2;
    if (index < 0) index = soundFileCount - 1;
    soundIndex = index % soundFileCount;
}

void updateVolume() {
    int vol = constrain(encoder.getCount() / 2, 0, 100);
    encoder.setCount(vol * 2);
    audio.setVolume(std::round(vol * 21 / 100));
}

void click(Button2 &btn) { updateMode(); }

void updateMode() {
    mode = (mode + 1) % 3;
    switch (mode) {
        case 0: encoder.setCount(metronomeSettings.bpm * 2); break;
        case 1: encoder.setCount(soundIndex * 2); break;
        case 2: encoder.setCount(audio.getVolume() * 100 / 21 * 2); break;
    }
}
void released(Button2 &btn) { Serial.println("Button released"); }
void longClickDetected(Button2 &btn) { metronomRunning = !metronomRunning; }
void longClick(Button2 &btn) { Serial.println("Long click"); }
void doubleClick(Button2 &btn) { Serial.println("Double click"); }
void tripleClick(Button2 &btn) { Serial.println("Triple click"); }
