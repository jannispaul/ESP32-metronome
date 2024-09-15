#include <Arduino.h>

#include <ESP32Encoder.h>  // https://github.com/madhephaestus/ESP32Encoder.git
#include "Button2.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "Arduino.h"
#include "Audio.h"  //ESP32-audioI2S/
//#include "SD.h"
#include "FS.h"
#include "SPIFFS.h"


#define CLK 13  // CLK ENCODER
#define DT 15   // DT ENCODER
#define BUTTON_PIN 2

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

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


//int analogValue = 0;
int bpm = 120;
int bpmMax = 240;
int bpmMin = 30;
int lastBpm = 0;
int LEDdelaytime = 100;

int LEDPin = 32;
//int potiPin = 36;
int pulseWidth = 50;
//int frequency = 0;
int triggerDistance = 0;
float temp = 0.0;
int displayRefresh = 500;

int SoundNo = 1;

bool metronomRunning = true;
bool displayMenu = false;
bool displayToggle = false;
bool LEDDelayActive;

// Functions
void pressed(Button2& btn);
void released(Button2& btn);
void changed(Button2& btn);
void click(Button2& btn);
void longClickDetected(Button2& btn);
void longClick(Button2& btn);
void doubleClick(Button2& btn);
void tripleClick(Button2& btn);
void tap(Button2& btn);
void LEDDelay();
void loop();

void setup(void) {

  Serial.begin(115200);
  SPIFFS.begin();
  Serial.println("Start");

  // Print file names in filesystem to serial
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  File root = SPIFFS.open("/");

  File file = root.openNextFile();

  while (file) {
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }

  // Enable the weak pull up resistors for encoder
  //ESP32Encoder::useInternalWeakPullResistors = UP;
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(DT, CLK);
  encoder.setCount(bpm * 2);

  button.begin(BUTTON_PIN);
  // button.setLongClickTime(1000);
  // button.setDoubleClickTime(400);

  Serial.println(" Longpress Time:\t" + String(button.getLongClickTime()) + "ms");
  Serial.println(" DoubleClick Time:\t" + String(button.getDoubleClickTime()) + "ms");
  Serial.println();

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
  //pinMode(potiPin, INPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  // Clear the buffer
  display.clearDisplay();
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();

  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.setCursor(15, 15);
  display.print("cooles");
  display.setCursor(15, 35);
  display.print(" - Metronom -");
  display.setCursor(15, 55);
  display.print("by FS & JPW");
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setTextSize(2);
  display.display();

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(6); // default 0...21
  //  or alternative
  // audio.setVolumeSteps(100);  // max 255
  audio.setVolume(22);
}

void bpmDisplay()

{

  if (displayMenu == true && displayToggle == true) {

    // print data on the SSD1306 display
    display.setCursor(0, 20);
    display.print("cooler sound");
    display.display();
    displayToggle = false;
  }

  if (millis() - timeStampDisplay > displayRefresh) {

    timeStampDisplay = millis();
    Serial.println(timeStampDisplay);
    //analogValue = analogRead(potiPin);
    //bpm = map(analogValue, 0, 4095, 30, 240);
    //bpm = encoderValue
    temp = (1 / (bpm / 60.0)) * 1000.0;
    triggerDistance = round(temp);
    Serial.println(triggerDistance);
    Serial.println(temp);
    Serial.println(bpm);

    if (displayToggle == true && displayMenu == false) {

      String bpmString = String(bpm);
      display.clearDisplay();
      display.setCursor(0, 20);
      display.print(bpmString);
      display.display();

      lastBpm = bpm;
      displayToggle = false;
    }

    if (bpm != lastBpm) {

      if (displayMenu == false) {
        String bpmString = String(bpm);

        display.clearDisplay();
        display.setCursor(0, 20);
        display.print(bpmString);
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


void audioClick() {
  if (SoundNo == 1) {
    audio.connecttoFS(SPIFFS, "/sound1.wav");
    // SD
  }

  if (SoundNo == 2) {
    audio.connecttoFS(SPIFFS, "/sound2.wav");
  }
}


void pressed(Button2& btn) {
  Serial.println("pressed");
}
void released(Button2& btn) {
  Serial.print("released: ");
  Serial.println(btn.wasPressedFor());
}
void changed(Button2& btn) {
  Serial.println("changed");
}
void click(Button2& btn) {
  Serial.println("click\n");
  displayMenu = !displayMenu;
  displayToggle = true;
  if (SoundNo < 2) {
    SoundNo++;
  } else {
    SoundNo = 1;
  }
}
void longClickDetected(Button2& btn) {
  Serial.println("long click detected");
  metronomRunning = !metronomRunning;
}
void longClick(Button2& btn) {
  Serial.println("long click\n");
}
void doubleClick(Button2& btn) {
  Serial.println("double click\n");
}
void tripleClick(Button2& btn) {
  Serial.println("triple click\n");
  Serial.println(btn.getNumberOfClicks());
}
void tap(Button2& btn) {
  Serial.println("tap");
}

void LEDDelay() {
  LEDDelayStart = millis();
  LEDDelayActive = true;
}

void loop() {

  bpmDisplay();
  //encoder
  if ((encoder.getCount() / 2) > bpmMax) {
    encoder.setCount(bpmMax * 2);
  }

  if ((encoder.getCount() / 2) < bpmMin) {
    encoder.setCount(bpmMin * 2);
  }

  bpm = encoder.getCount() / 2;
  //Serial.println("Encoder count = " + String((int32_t)encoder.getCount()));
  //Serial.println("bpmcount = " + String(bpm));

  button.loop();


  if (millis() - timeStampBPM > triggerDistance) {

    if (metronomRunning == true) {
      audioClick();
      LEDDelay();
    }
    timeStampBPM = millis();
  }

  if ((millis() - LEDDelayStart > LEDdelaytime) && LEDDelayActive == true) {
    pulseLED();
    LEDDelayActive = false;
  }

  if (millis() - timeStampLED > pulseWidth) {
    digitalWrite(LEDPin, LOW);
  }

  audio.loop();
}
