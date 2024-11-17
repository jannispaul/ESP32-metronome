#include <Arduino.h>

#include <ESP32Encoder.h> // https://github.com/madhephaestus/ESP32Encoder.git
#include "Button2.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> // Display Library
#include <U8g2lib.h>          // UI Library

#include "Arduino.h"
#include "Audio.h" //ESP32-audioI2S/
// #include "SD.h"
#include "FS.h"
#include "SPIFFS.h"

#define CLK 13 // CLK ENCODER
#define DT 15  // DT ENCODER
#define BUTTON_PIN 2

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET LED_BUILTIN
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define I2S_DOUT 25
#define I2S_BCLK 26
#define I2S_LRC 27

ESP32Encoder encoder;
Button2 button;
Audio audio;

static uint64_t timeStampDisplay = 0;
static uint64_t timeStampLED = 0;
static uint64_t timeStampBPM = 0;

unsigned long lastMillis = 0;
unsigned long LEDDelayStart;
unsigned long LEDDPulseStart;

// int analogValue = 0;
int bpm = 120;
int bpmMax = 240;
int bpmMin = 30;
int lastBpm = 0;
int LEDdelaytime = 100;
int mode = 0; // 0 = bpm, 1 = sound selection

int LEDPin = 32;
// int potiPin = 36;
int pulseWidth = 50;
// int frequency = 0;
int triggerDistance = 0;
float temp = 0.0;
int displayRefresh = 500;

int soundIndex = 0;

const char *soundFiles[] = {
    "/sound1.wav", // Index 0
    "/sound2.wav"};
int soundFileCount = sizeof(soundFiles) / sizeof(soundFiles[0]);

bool metronomRunning = true;
bool displayMenu = false;
bool displayToggle = false;
bool LEDDelayActive;

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

// U8G2 Constructor for ESP32
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
// UI Bitmaps
// 'Property 1=BPM', 16x16px
const unsigned char epd_bitmap_Property_1_BPM[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x31, 0xba, 0xb5, 0x92, 0x21, 0xaa,
    0xad, 0xbb, 0xad, 0xbb, 0xa1, 0xbb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Minus', 16x16px
const unsigned char epd_bitmap_Property_1_Minus[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xf0,
    0x0f, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Metronome', 16x16px
const unsigned char epd_bitmap_Property_1_Metronome[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0x7f, 0xfe, 0xbf, 0xfd, 0xdf, 0xfb, 0xdf, 0xfb, 0xdf, 0xfb, 0xef, 0xf7,
    0xef, 0xf7, 0xef, 0xf7, 0xf7, 0xef, 0xf7, 0xef, 0xf7, 0xef, 0x07, 0xe0, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Measure', 16x16px
const unsigned char epd_bitmap_Property_1_Measure[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xfd, 0xff, 0xc1, 0xff, 0xff, 0xfb, 0xff, 0xfd, 0xff, 0xfe,
    0x7f, 0xff, 0xbf, 0xff, 0xdf, 0xbf, 0xff, 0xbf, 0xff, 0x83, 0xff, 0xbf, 0xff, 0xbf, 0xff, 0xff};
// 'Property 1=Volume 33', 16x16px
const unsigned char epd_bitmap_Property_1_Volume_33[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xff, 0x67, 0xff, 0xe1, 0xfe, 0xe1, 0xfe,
    0xe1, 0xfe, 0xe1, 0xfe, 0x67, 0xff, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Volume', 16x16px
const unsigned char epd_bitmap_Property_1_Volume[] PROGMEM = {
    0xff, 0xff, 0xff, 0xf7, 0xff, 0xef, 0xff, 0xdd, 0xef, 0xbb, 0x67, 0xb7, 0xe1, 0xb6, 0xe1, 0xb6,
    0xe1, 0xb6, 0xe1, 0xb6, 0x67, 0xb7, 0xef, 0xbb, 0xff, 0xdd, 0xff, 0xef, 0xff, 0xf7, 0xff, 0xff};
// 'Property 1=Plus', 16x16px
const unsigned char epd_bitmap_Property_1_Plus[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x0f, 0xf0,
    0x0f, 0xf0, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Volume 66', 16x16px
const unsigned char epd_bitmap_Property_1_Volume_66[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xef, 0xfb, 0x67, 0xf7, 0xe1, 0xf6, 0xe1, 0xf6,
    0xe1, 0xf6, 0xe1, 0xf6, 0x67, 0xf7, 0xef, 0xfb, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Bar', 16x16px
const unsigned char epd_bitmap_Property_1_Bar[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x63, 0xe4, 0x6b, 0xd5, 0x43, 0xe4,
    0x5b, 0xd5, 0x5b, 0xd5, 0x43, 0xd5, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Battery 50', 16x16px
const unsigned char epd_bitmap_Property_1_Battery_50[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0xe0, 0xfb, 0xef, 0xab, 0xcf,
    0xab, 0xcf, 0xfb, 0xef, 0x03, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Battery 0', 16x16px
const unsigned char epd_bitmap_Property_1_Battery_0[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0xe0, 0xfb, 0xef, 0xfb, 0xcf,
    0xfb, 0xcf, 0xfb, 0xef, 0x03, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Battery 75', 16x16px
const unsigned char epd_bitmap_Property_1_Battery_75[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0xe0, 0xfb, 0xef, 0xab, 0xce,
    0xab, 0xce, 0xfb, 0xef, 0x03, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Battery 25', 16x16px
const unsigned char epd_bitmap_Property_1_Battery_25[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0xe0, 0xfb, 0xef, 0xeb, 0xcf,
    0xeb, 0xcf, 0xfb, 0xef, 0x03, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Beat', 16x16px
const unsigned char epd_bitmap_Property_1_Battery_100[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0xe0, 0xfb, 0xef, 0xab, 0xca,
    0xab, 0xca, 0xfb, 0xef, 0x03, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Increase', 16x16px
const unsigned char epd_bitmap_Property_1_Beat[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9d, 0x88, 0xdd, 0xda, 0x91, 0xda,
    0xd5, 0xd8, 0xd5, 0xda, 0x91, 0xda, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Battery 100', 16x16px
const unsigned char epd_bitmap_Property_1_Increase[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xff, 0xe7, 0xff, 0xe3, 0xff, 0xe1, 0xff, 0xe0,
    0x7f, 0xe0, 0x3f, 0xe0, 0x1f, 0xe0, 0x0f, 0xe0, 0x07, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 'Property 1=Increase off', 16x16px
const unsigned char epd_bitmap_Property_1_Increase_off[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xff, 0xe7, 0xff, 0xeb, 0xff, 0xed, 0xff, 0xee,
    0x7f, 0xef, 0xbf, 0xef, 0xdf, 0xef, 0xef, 0xef, 0x07, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 816)
const int epd_bitmap_allArray_LEN = 17;
const unsigned char *epd_bitmap_allArray[17] = {
    epd_bitmap_Property_1_BPM,
    epd_bitmap_Property_1_Bar,
    epd_bitmap_Property_1_Battery_0,
    epd_bitmap_Property_1_Battery_100,
    epd_bitmap_Property_1_Battery_25,
    epd_bitmap_Property_1_Battery_50,
    epd_bitmap_Property_1_Battery_75,
    epd_bitmap_Property_1_Beat,
    epd_bitmap_Property_1_Increase,
    epd_bitmap_Property_1_Increase_off,
    epd_bitmap_Property_1_Measure,
    epd_bitmap_Property_1_Metronome,
    epd_bitmap_Property_1_Minus,
    epd_bitmap_Property_1_Plus,
    epd_bitmap_Property_1_Volume,
    epd_bitmap_Property_1_Volume_33,
    epd_bitmap_Property_1_Volume_66};

static const unsigned char image_Property_1_Battery_100_bits[] U8X8_PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x1f, 0x04, 0x10, 0x54, 0x35, 0x54, 0x35, 0x04, 0x10, 0xfc, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const unsigned char image_Property_1_Volume_bits[] U8X8_PROGMEM = {0x00, 0x00, 0x00, 0x08, 0x00, 0x10, 0x00, 0x22, 0x10, 0x44, 0x98, 0x48, 0x1e, 0x49, 0x1e, 0x49, 0x1e, 0x49, 0x1e, 0x49, 0x98, 0x48, 0x10, 0x44, 0x00, 0x22, 0x00, 0x10, 0x00, 0x08, 0x00, 0x00};

static const unsigned char *image_Property_1_Battery_75_bits = epd_bitmap_Property_1_Battery_50;

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
    // if (displayMenu == false)
    // {
    // }
    if (displayMenu == true && displayToggle == true)
    {
        // displayUI(1);
        // print data on the SSD1306 display
        // display.setCursor(0, 20);
        // display.print("cooler sound");
        // display.display();
        displayToggle = false;
    }

    if (millis() - timeStampDisplay > displayRefresh)
    {

        timeStampDisplay = millis();
        Serial.println(timeStampDisplay);
        // analogValue = analogRead(potiPin);
        // bpm = map(analogValue, 0, 4095, 30, 240);
        // bpm = encoderValue
        temp = (1 / (bpm / 60.0)) * 1000.0;
        triggerDistance = round(temp);
        Serial.println("triggerDistance: " + String(triggerDistance));
        Serial.println("tempo: " + String(temp));
        Serial.println("BPM: " + String(bpm));

        if (displayToggle == true && displayMenu == false)
        {
            String bpmString = String(bpm);
            display.clearDisplay();
            display.setCursor(0, 20);
            display.print(bpmString);
            display.display();

            lastBpm = bpm;
            displayToggle = false;
        }

        if (bpm != lastBpm)
        {
            if (displayMenu == false)
            {
                String bpmString = String(bpm);
                display.print(bpmString);
                display.clearDisplay();
                display.setCursor(0, 20);
                display.display();
                lastBpm = bpm;
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
// void audioClick()
// {
//     if (soundIndex == 1)
//     {
//         audio.connecttoFS(SPIFFS, "/sound1.wav");
//         // SD
//     }

//     if (soundIndex == 2)
//     {
//         audio.connecttoFS(SPIFFS, "/sound2.wav");
//     }
// }

void pressed(Button2 &btn)
{
    Serial.println("pressed");
}
void released(Button2 &btn)
{
    Serial.print("released: ");
    Serial.println(btn.wasPressedFor());
}
void changed(Button2 &btn)
{
    Serial.println("changed");
}
void click(Button2 &btn)
{
    Serial.println("click\n");
    displayMenu = !displayMenu;
    displayToggle = true;
    mode = !mode;
    // if (soundIndex < 2)
    // {
    //     soundIndex++;
    // }
    // else
    // {
    //     soundIndex = 1;
    // }
}
void longClickDetected(Button2 &btn)
{
    Serial.println("long click detected");
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
        String bpmString = String(bpm);
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
    // BPM Mode
    if (mode == 0)
    {
        // Keep max and min value on encoder
        if ((encoder.getCount() / 2) > bpmMax)
        {
            encoder.setCount(bpmMax * 2);
        }

        if ((encoder.getCount() / 2) < bpmMin)
        {
            encoder.setCount(bpmMin * 2);
        }
        bpm = encoder.getCount() / 2;
        // Serial.println("Encoder count = " + String((int32_t)encoder.getCount()));
        // Serial.println("bpmcount = " + String(bpm));
    }
    // Sound selection mode
    else if (mode == 1)
    {
        soundIndex = encoder.getCount() / 2 % soundFileCount;
    }
}
void loop()
{
    displayBPM();
    handleEncoder();

    button.loop();

    if (millis() - timeStampBPM > triggerDistance)
    {

        if (metronomRunning == true)
        {
            audioClick(soundIndex);
            LEDDelay();
        }
        timeStampBPM = millis();
    }

    if ((millis() - LEDDelayStart > LEDdelaytime) && LEDDelayActive == true)
    {
        pulseLED();
        LEDDelayActive = false;
    }

    if (millis() - timeStampLED > pulseWidth)
    {
        digitalWrite(LEDPin, LOW);
    }

    audio.loop();
}
