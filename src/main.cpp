#include <Arduino.h>
#include <ESP32Encoder.h>
#include "Button2.h"
#include <Wire.h>
//#include <Adafruit_GFX.h>
// #include <Adafruit_SSD1306.h>
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
// #define OLED_RESET LED_BUILTIN
// Adafruit_SSD1306 display(DisplayConfig::SCREEN_WIDTH, DisplayConfig::SCREEN_HEIGHT, &Wire, OLED_RESET);

// === Sound ===
const char *soundFiles[MAX_SOUND_FILES];
int soundFileCount;
int soundIndex = 0;

// === State Flags ===
bool LEDDelayActive = true;
bool metronomRunning = true;
int mode = 0;

int lastMode = -1;
int lastEncoderValue = 0;
int lastEncoderRawCount = 0;

int volumePercent = 50; // start at 50%


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
bool shouldTriggerMetronome(unsigned long now);
void triggerMetronome(unsigned long now);
bool shouldTurnOffLED(unsigned long now);
bool shouldPulseLED();
void pulseLED(unsigned long now);
void LEDDelay();
void audioClick(int soundIndex);


void setup() {
    Wire.begin(PinConfig::I2C_SDA, PinConfig::I2C_SCL); // Initialize I2C
    u8g2.begin(); // Initialize OLED
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
    encoder.attachFullQuad(PinConfig::DT, PinConfig::CLK);
    encoder.setFilter(256);

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

    // Audio setup
    audio.setPinout(PinConfig::I2S_BCLK, PinConfig::I2S_LRC, PinConfig::I2S_DOUT);
    audio.setVolume(metronomeSettings.volume); // Set initial volume (10 of 21 = ~50%)

    // Sync encoder to BPM (default mode 0)
    encoder.setCount(metronomeSettings.bpm * 4);
    lastEncoderRawCount = encoder.getCount();

    // Calculate initial beat interval based on BPM
    metronomeSettings.updateBeatInterval();


    // Initialize volume percent for display
    volumePercent = round(metronomeSettings.volume * 100.0 / 21.0);

    // Initial debug output
    Serial.print("Initial BPM: ");
    Serial.println(metronomeSettings.bpm);

    Serial.print("Initial volume: ");
    Serial.print(audio.getVolume());
    Serial.println(" of 21");

    Serial.print("volumePercent: ");
    Serial.println(volumePercent);

    Serial.print("encoder count: ");
    Serial.println(encoder.getCount());
}

void loop() {
    unsigned long now = millis();
    handleMode();  // handle encoder input and update UI if needed
    button.loop();

    // Trigger metronome if enough time has passed since last trigger
    if (shouldTriggerMetronome(now)) {
        triggerMetronome(now);
    }

    // Turn off LED if pulse duration elapsed
    if (digitalRead(PinConfig::LED_PIN) == HIGH) {
        unsigned long diff = now - timingConfig.ledTimestamp;
        
        if (shouldTurnOffLED(now)) {
            digitalWrite(PinConfig::LED_PIN, LOW);
            Serial.println("LED OFF");
            Serial.printf("LED was ON for %lu ms\n", diff);
        }
    }

     audio.loop();  
}

bool shouldTriggerMetronome(unsigned long now) {
    return (now - timingConfig.bpmTimestamp) >= metronomeSettings.triggerDistance;
}

void triggerMetronome(unsigned long now) {
    Serial.println("Metronome Triggered");
    pulseLED(now);
    audioClick(soundIndex);
    timingConfig.bpmTimestamp = now;
}

void audioClick(int soundIndex) {
    if (soundIndex >= 0 && soundIndex < soundFileCount) {
        audio.connecttoFS(SPIFFS, soundFiles[soundIndex]);
    }
}
void pulseLED(unsigned long now) {
    unsigned long diff = now - timingConfig.ledTimestamp;
    timingConfig.ledTimestamp = now;
    digitalWrite(PinConfig::LED_PIN, HIGH);
    Serial.printf("LED ON (delta since last HIGH: %lu ms)\n", diff);
    Serial.printf("pulseWidth: %d ms\n", metronomeSettings.pulseWidth);
}

bool shouldPulseLED() {
    return LEDDelayActive && millis() - timingConfig.LEDDelayStart > metronomeSettings.ledDelayTime;
}

bool shouldTurnOffLED(unsigned long now) {
    return now - timingConfig.ledTimestamp > metronomeSettings.pulseWidth;
}

void LEDDelay() {
    timingConfig.LEDDelayStart = millis();
    LEDDelayActive = true;
}

void maybeUpdateUI() {
    unsigned long now = millis();
    if (now - timingConfig.displayTimestamp > 200) {  // slower for better pulse performance
        updateUI();
        timingConfig.displayTimestamp = now;
    }
}

void handleMode() {
    handleEncoder();
    maybeUpdateUI();  // instead of directly updateUI()
}




void updateUI() {
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

        String volumeString = String(volumePercent) + "%";
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
    if (mode != lastMode) {
        Serial.print("Mode: "); Serial.println(mode);
        lastMode = mode;
    }

    switch (mode) {
        case 0: updateBPM(); break;
        case 1: selectSound(); break;
        case 2: updateVolume(); break;
    }
}


void updateBPM() {
    long rawCount = encoder.getCount();            // Read raw encoder count (FullQuad)
    long diff = rawCount - lastEncoderRawCount;    // Calculate movement since last update
    int bpmChange = diff / 4;                       // Convert ticks to BPM steps (1 step = 4 ticks)

    if (bpmChange != 0) {
        // Calculate new BPM value within allowed range
        int newBpm = constrain(metronomeSettings.bpm + bpmChange, metronomeSettings.bpmMin, metronomeSettings.bpmMax);

        if (newBpm != metronomeSettings.bpm) {
            metronomeSettings.bpm = newBpm;        // Update BPM setting
            Serial.print("BPM changed to: ");
            Serial.println(newBpm);
            metronomeSettings.updateBeatInterval(); // Update timing based on new BPM
        }

        // Reset encoder count to match new BPM scaled by 4
        encoder.setCount(newBpm * 4);

        // Update last encoder raw count for future diffs
        lastEncoderRawCount = encoder.getCount();
    }
}




void selectSound() {
    int value = encoder.getCount() / 4;
    if (value != lastEncoderValue) {
        int index = (value % soundFileCount + soundFileCount) % soundFileCount;
        if (index != soundIndex) {
            soundIndex = index;
            encoder.setCount(index * 4);
            Serial.print("Sound index: ");
            Serial.println(soundIndex);
        }
        lastEncoderValue = value;
    }
}


void updateVolume() {
    long rawCount = encoder.getCount();  // Get raw encoder count (FullQuad)
    long diff = rawCount - lastEncoderRawCount;  // Calculate movement since last update
    int volChange = diff / 4;  // Convert ticks to volume steps (1 step = 4 ticks)

    if (volChange != 0) {
        // Update volume percentage, constrained between 0 and 100%
        volumePercent = constrain(volumePercent + volChange, 0, 100);

        // Map volumePercent (0-100) to audio volume range (0-21)
        int newVolume = round(volumePercent * 21.0 / 100.0);

        // Set audio volume
        audio.setVolume(newVolume);

        // Reset encoder count to match current volumePercent * 4 (scaled)
        encoder.setCount(volumePercent * 4);

        // Update last encoder raw count for next diff calculation
        lastEncoderRawCount = encoder.getCount();

        // Debug print
        Serial.print("Volume: ");
        Serial.print(volumePercent);
        Serial.println(" %");
    }
}






void click(Button2 &btn) { 
    Serial.println("Button clicked"); 
    updateMode(); 
}

void updateMode() {
    mode = (mode + 1) % 3;
    switch (mode) {
        case 0:
            encoder.setCount(metronomeSettings.bpm * 4);
            lastEncoderValue = metronomeSettings.bpm;
            break;
        case 1:
            encoder.setCount(soundIndex * 4);
            lastEncoderValue = soundIndex;
            break;
        case 2:
            volumePercent = round(audio.getVolume() * 100.0 / 21.0);  // Startwert setzen
            encoder.setCount(volumePercent * 4);
            lastEncoderValue = volumePercent;
            break;

    }
}

void released(Button2 &btn) { Serial.println("Button released"); }
void longClickDetected(Button2 &btn) { metronomRunning = !metronomRunning; }
void longClick(Button2 &btn) { Serial.println("Long click"); }
void doubleClick(Button2 &btn) { Serial.println("Double click"); }
void tripleClick(Button2 &btn) { Serial.println("Triple click"); }
