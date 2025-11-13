#pragma once
#include <Arduino.h>

// I2S-Pins (MAX98357A)
constexpr uint8_t I2S_DOUT = 25;
constexpr uint8_t I2S_BCLK = 26;
constexpr uint8_t I2S_LRC  = 27;

// Initialisiert die AudioTask (Core 1) und I2S (einmalig)
void audio_init();

// BPM festlegen (z. B. 60.0). Untergrenze 1 BPM.
// Darf auch vor audio_init() aufgerufen werden.
void audio_set_bpm(float bpm);

// Übergibt einen im RAM liegenden WAV-Buffer (inkl. 44-Byte WAV-Header).
// Format-Annahme: PCM 16-bit mono, SAMPLE_RATE (siehe audio.cpp).
// Darf auch vor audio_init() aufgerufen werden.
void audio_set_buffer(uint8_t* data, size_t size);

// Spielt die AudioTask aktuell?
bool audio_is_playing();

// Blockiert bis ein "Playback fertig"-Event eintrifft (Timeout in ms).
// Liefert true bei Event, false bei Timeout oder wenn Queue noch nicht existiert.
bool audio_take_playback_done(uint32_t timeout_ms);