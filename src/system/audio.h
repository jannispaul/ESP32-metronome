#pragma once
#include <Arduino.h>

// I2S-Pins (MAX98357A)
constexpr uint8_t I2S_DOUT = 25;
constexpr uint8_t I2S_BCLK = 26;
constexpr uint8_t I2S_LRC  = 27;

// ---- Init & Beat ----
void     audio_init();
void     audio_set_bpm(uint16_t bpm);
uint16_t audio_get_bpm();
uint32_t audio_get_period_ms();

// ---- Buffer Übergabe ----
void audio_set_buffer(uint8_t* data, size_t size);

// ---- Status / Events ----
bool audio_is_playing();
bool audio_take_playback_done(uint32_t timeout_ms);

// ---- Hold (Start-Sperre) ----
void audio_set_hold(bool hold);
bool audio_get_hold();

// ---- NEU: Lautstärke ----
// Prozent 0..100 (0 = aus, 100 = unverändert)
void    audio_set_volume(uint8_t percent);
uint8_t audio_get_volume();

// Mute toggeln/abfragen (Mute hat Vorrang vor Volume)
void audio_mute(bool enable);
bool audio_is_muted();


// ---- NEU: Beat-Queue (Sync für LED) ----
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
QueueHandle_t audio_get_beat_queue();

//
void audio_commit_bpm_now(uint16_t bpm);
void audio_reset_beat_sync_now();
