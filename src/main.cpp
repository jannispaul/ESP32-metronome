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
#include "SoundFileLoader.h"
#include "bitmaps.h"
#include "Config.h"

// Create instances of configuration structures
MetronomeSettings metronomeSettings;
TimingConfig timingConfig;

// Hardware instances
ESP32Encoder encoder;
Button2 button;
Audio audio;

// Display setup
#define OLED_RESET LED_BUILTIN
Adafruit_SSD1306 display(DisplayConfig::SCREEN_WIDTH, DisplayConfig::SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sound configuration
int soundIndex = 0;
const char *soundFiles[MAX_SOUND_FILES];
int soundFileCount;

bool LEDDelayActive = false;

// LED variables
int LEDdelaytime = 100;
int pulseWidth = 50;

// Mode variable
int mode = 0; // 0 = bpm, 1 = sound selection, 2 = volume

// Metronome state
bool metronomRunning = true;

// Functions declarations
void pressed(Button2 &btn);
void released(Button2 &btn);
void changed(Button2 &btn);
void click(Button2 &btn);
void longClickDetected(Button2 &btn);
void longClick(Button2 &btn);
void doubleClick(Button2 &btn);
void tripleClick(Button2 &btn);
void tap(Button2 &btn);
void LEDDelay();
void loop();
void updateUI();
void toggleMetronomeRunningState();
void handleEncoder();
void updateBPM();
void selectSound();
bool shouldUpdateDisplay();
void logDisplayInfo();
void updateDisplayWithBPM();
bool shouldTriggerMetronome();
void triggerMetronome();
bool shouldPulseLED();
void pulseLED();
bool shouldTurnOffLED();
void updateVolume();
void updateMode();

void setup(void)
{
    u8g2.begin();
    Serial.begin(115200);
    SPIFFS.begin();
    // delay(6000); // waits for 6 seconds
    Serial.println("Start");

    // Print file names in filesystem to serial
    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    File root = SPIFFS.open("/");

    File file = root.openNextFile();

    while (file)
    {
        Serial.print("FILE: ");
        Serial.println(file.name());
        file = root.openNextFile();
    }
    // Load sound files
    soundFileCount = loadSoundFiles(soundFiles, MAX_SOUND_FILES);
    Serial.printf("%d sound files loaded.\n", soundFileCount);

    // Enable the weak pull up resistors for encoder
    // ESP32Encoder::useInternalWeakPullResistors = UP;
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(PinConfig::DT, PinConfig::CLK);

    button.begin(PinConfig::BUTTON_PIN);
    button.setLongClickTime(1000);
    // button.setDoubleClickTime(400);

    Serial.println(" Longpress Time:\t" + String(button.getLongClickTime()) + "ms");
    Serial.println(" DoubleClick Time:\t" + String(button.getDoubleClickTime()) + "ms");

    // button.setChangedHandler(changed);
    // button.setPressedHandler(pressed);
    button.setReleasedHandler(released);

    // button.setTapHandler(tap);
    button.setClickHandler(click);
    button.setLongClickDetectedHandler(longClickDetected);
    button.setLongClickHandler(longClick);
    button.setLongClickDetectedRetriggerable(false);

    button.setDoubleClickHandler(doubleClick);
    button.setTripleClickHandler(tripleClick);

    pinMode(PinConfig::LED_PIN, OUTPUT);
    digitalWrite(PinConfig::LED_PIN, LOW);
    // pinMode(potiPin, INPUT);

    // Display failsafe
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    { // Address 0x3D for 128x64
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }

    // Audio Setup
    audio.setPinout(PinConfig::I2S_BCLK, PinConfig::I2S_LRC, PinConfig::I2S_DOUT);
    audio.setVolume(6); // default 0...21
    //  or alternative

    // About 50% volume
    audio.setVolume(metronomeSettings.volume);

    // Set encoder to initial bpm
    encoder.setCount(metronomeSettings.bpm * 2);
}

void handleMode()
{
    handleEncoder();
    updateUI();
    logDisplayInfo();
}

void logDisplayInfo()
{
    Serial.println(timingConfig.displayTimestamp);
    metronomeSettings.updateTriggerDistance();
    Serial.println("triggerDistance: " + String(metronomeSettings.triggerDistance));
    Serial.println("BPM: " + String(metronomeSettings.bpm));
    Serial.println("mode: " + String(mode));
}

void pulseLED()
{
    timingConfig.ledTimestamp = millis();
    digitalWrite(PinConfig::LED_PIN, HIGH);
}

void audioClick(int soundIndex)
{
    if (soundIndex >= 0 && soundIndex < sizeof(soundFiles) / sizeof(soundFiles[0]))
    {
        audio.connecttoFS(SPIFFS, soundFiles[soundIndex]);
    }
    else
    {
        Serial.println("Invalid sound index!");
    }
}

void pressed(Button2 &btn)
{
    // Log the button press event
    Serial.println("Button pressed");
}

void released(Button2 &btn)
{
    // Log the button release event and the duration it was pressed
    Serial.print("Button released after: ");
    Serial.println(btn.wasPressedFor());
}

void changed(Button2 &btn)
{
    Serial.println("changed");
}

void click(Button2 &btn)
{
    // Log the button click event
    Serial.println("Button clicked");

    updateMode();
}
// change mode function
void updateMode()
{

    // Go through modes 0 = bpm, 1 = sound, 2 = volume
    mode = (mode + 1) % 3;
    // Set encoder to last value of the mode
    if (mode == 0)
    {
        encoder.setCount(metronomeSettings.bpm * 2);
        Serial.println("updateMode: bpm: " + String(metronomeSettings.bpm));
    }
    else if (mode == 1)
    {
        encoder.setCount(soundIndex * 2);
        Serial.println("updateMode: sound: " + String(soundIndex));
    }
    else if (mode == 2)
    {
        uint8_t volume = audio.getVolume();
        encoder.setCount(volume * metronomeSettings.volumeFactor * 2);
        Serial.println("updateMode: volume: " + String(volume));
    }
}

void longClickDetected(Button2 &btn)
{
    Serial.println("Long click detected");

    // Toggle the metronome running state
    toggleMetronomeRunningState();
}

void toggleMetronomeRunningState()
{
    metronomRunning = !metronomRunning;
}

void longClick(Button2 &btn)
{
    Serial.println("long click\n");
}

void doubleClick(Button2 &btn)
{
    Serial.println("double click\n");
}

void tripleClick(Button2 &btn)
{
    Serial.println("triple click\n");
    Serial.println(btn.getNumberOfClicks());
}

void tap(Button2 &btn)
{
    Serial.println("tap");
}

void LEDDelay()
{
    timingConfig.LEDDelayStart = millis();
    LEDDelayActive = true;
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

void handleEncoder()
{
    if (mode == 0)
    {
        updateBPM();
    }
    else if (mode == 1)
    {
        selectSound();
    }
    else if (mode == 2)
    {
        updateVolume();
    }
}

void updateVolume()
{

    int encoderValue = encoder.getCount() / 2;

    Serial.println("Volume encoderValue: " + String(encoderValue));
    // Constrain volume between 0 and 100
    if (encoderValue > 100)
    {
        encoder.setCount(100 * 2);
    }
    else if (encoderValue < 0)
    {
        encoder.setCount(0);
    }
    // Translate encoder value to volume (0-21) with linear speed adjustment
    audio.setVolume(std::round(encoderValue * 21 / 100));
}

void updateBPM()
{

    int encoderValue = encoder.getCount() / 2;
    Serial.println("BPM encoderValue: " + String(encoderValue));

    // Make sure the encoder value is within the valid range
    if (encoderValue > metronomeSettings.bpmMax)
    {
        encoder.setCount(metronomeSettings.bpmMax * 2);
    }
    else if (encoderValue < metronomeSettings.bpmMin)
    {
        encoder.setCount(metronomeSettings.bpmMin * 2);
    }

    metronomeSettings.bpm = encoderValue;
}

void selectSound()
{
    // prevent when encode becomes negative start from the end
    if (encoder.getCount() < 0)
    {
        soundIndex = soundFileCount - 1;
    }
    else
    {
        soundIndex = (encoder.getCount() / 2) % soundFileCount;
    }
}

void loop()
{

    handleMode();
    button.loop();

    if (shouldTriggerMetronome())
    {
        triggerMetronome();
    }

    if (shouldPulseLED())
    {
        pulseLED();
        LEDDelayActive = false;
    }

    if (shouldTurnOffLED())
    {
        digitalWrite(PinConfig::LED_PIN, LOW);
    }

    audio.loop();
}

bool shouldTriggerMetronome()
{
    return millis() - timingConfig.bpmTimestamp > metronomeSettings.triggerDistance;
}

void triggerMetronome()
{
    if (metronomRunning)
    {
        audioClick(soundIndex);
        digitalWrite(PinConfig::LED_PIN, HIGH);
        timingConfig.ledTimestamp = millis();
        LEDDelay();
    }
    timingConfig.bpmTimestamp = millis();
}

bool shouldPulseLED()
{
    return (millis() - timingConfig.LEDDelayStart > metronomeSettings.ledDelayTime) && LEDDelayActive;
}

bool shouldTurnOffLED()
{
    return millis() - timingConfig.ledTimestamp > metronomeSettings.pulseWidth;
}
