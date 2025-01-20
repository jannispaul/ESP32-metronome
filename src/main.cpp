#include <Arduino.h>
#include <ESP32Encoder.h>
#include "Button2.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2lib.h>
#include "Arduino.h"
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

// Pin definitions
#define CLK 13 // CLK ENCODER
#define DT 15  // DT ENCODER
#define BUTTON_PIN 2
#define I2S_DOUT 25
#define I2S_BCLK 26
#define I2S_LRC 27
#define LEDPin 32

// Sound configuration
int soundIndex = 0;
const char *soundFiles[MAX_SOUND_FILES];
int soundFileCount;

// Display state
bool displayMenu = false;
bool displayToggle = false;
bool LEDDelayActive = false;

// Timing variables
static uint64_t timeStampDisplay = 0;
static uint64_t timeStampLED = 0;
static uint64_t timeStampBPM = 0;

unsigned long lastMillis = 0;
unsigned long LEDDelayStart;
unsigned long LEDDPulseStart;

// BPM variables
int bpm = 120;
int bpmMax = 240;
int bpmMin = 30;
int lastBpm = 0;

// LED variables
int LEDdelaytime = 100;
int pulseWidth = 50;

// Mode variable
int mode = 0; // 0 = bpm, 1 = sound selection

// Metronome state
bool metronomRunning = true;

// Functions
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
void displayUI(int displaySetting);
void toggleMetronomeState();
void handleEncoder();
void adjustBPM();
void selectSound();

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
    encoder.attachHalfQuad(DT, CLK);
    encoder.setCount(bpm * 2);

    button.begin(BUTTON_PIN);
    // button.setLongClickTime(1000);
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

    pinMode(LEDPin, OUTPUT);
    digitalWrite(LEDPin, LOW);
    // pinMode(potiPin, INPUT);

    // Display failsafe
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    { // Address 0x3D for 128x64
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }

    // Audio Setup
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(6); // default 0...21
    //  or alternative
    // audio.setVolumeSteps(100);  // max 255
    audio.setVolume(22);
}

void displayBPM()
{
    displayUI(mode);
    if (displayMenu == true && displayToggle == true)
    {
        displayToggle = false;
    }

    if (millis() - timeStampDisplay > DisplayConfig::REFRESH_RATE)
    {
        timeStampDisplay = millis();
        Serial.println(timeStampDisplay);

        metronomeSettings.updateTriggerDistance();
        Serial.println("triggerDistance: " + String(metronomeSettings.triggerDistance));
        Serial.println("BPM: " + String(metronomeSettings.bpm));

        if (displayToggle == true && displayMenu == false)
        {
            String bpmString = String(metronomeSettings.bpm);
            display.clearDisplay();
            display.setCursor(0, 20);
            display.print(bpmString);
            display.display();

            lastBpm = metronomeSettings.bpm;
            displayToggle = false;
        }

        if (metronomeSettings.bpm != lastBpm)
        {
            if (displayMenu == false)
            {
                String bpmString = String(metronomeSettings.bpm);
                display.print(bpmString);
                display.clearDisplay();
                display.setCursor(0, 20);
                display.display();
                lastBpm = metronomeSettings.bpm;
            }
        }
    }
}

void pulseLED()
{
    timeStampLED = millis();
    digitalWrite(LEDPin, HIGH);
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

    // Toggle the display menu and mode
    displayMenu = !displayMenu;
    displayToggle = true;
    mode = !mode;

    // Uncomment and adjust the logic below if sound index cycling is needed
    // if (soundIndex < 2) {
    //     soundIndex++;
    // } else {
    //     soundIndex = 1;
    // }
}

void longClickDetected(Button2 &btn)
{
    Serial.println("Long click detected");

    // Toggle the metronome running state
    toggleMetronomeState();
}

void toggleMetronomeState()
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
    LEDDelayStart = millis();
    LEDDelayActive = true;
}

void displayUI(int displayMode)
{
    // Setup display
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    // Draw ui based on displayMode
    if (displayMode == 0)
    {
        // Set bpm as string
        u8g2.setFont(u8g_font_profont29);
        String bpmString = String(metronomeSettings.bpm);
        u8g2.drawStr(57, 41, bpmString.c_str()); // Draw tempo on the display
    }
    else if (displayMode == 1)
    {
        u8g2.setFont(u8g_font_profont29);
        String soundString = "S" + String(soundIndex);
        u8g2.drawStr(57, 41, soundString.c_str()); // Draw tempo on the display

        // u8g2.drawXBM(0, 48, 16, 16, image_Property_1_Battery_75_bits);
        u8g2.setFont(u8g_font_5x7); // Change this to the correct font name
        u8g2.drawStr(36, 59, "Select a sound");
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
        adjustBPM();
    }
    else if (mode == 1)
    {
        selectSound();
    }
}

void adjustBPM()
{
    int encoderValue = encoder.getCount() / 2;
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
    soundIndex = (encoder.getCount() / 2) % soundFileCount;
}

void loop()
{
    displayBPM();
    handleEncoder();

    button.loop();

    if (millis() - timeStampBPM > metronomeSettings.triggerDistance)
    {
        if (metronomRunning == true)
        {
            audioClick(soundIndex);
            digitalWrite(LEDPin, HIGH);
            timeStampLED = millis();
            LEDDelay();
        }
        timeStampBPM = millis();
    }

    if ((millis() - LEDDelayStart > metronomeSettings.ledDelayTime) && LEDDelayActive == true)
    {
        pulseLED();
        LEDDelayActive = false;
    }

    if (millis() - timeStampLED > metronomeSettings.pulseWidth)
    {
        digitalWrite(LEDPin, LOW);
    }

    audio.loop();
}
