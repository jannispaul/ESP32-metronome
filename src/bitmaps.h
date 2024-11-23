#ifndef BITMAPS_H
#define BITMAPS_H

#include <Arduino.h>
#include <U8g2lib.h>

// Declare the U8G2 object
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Declare bitmap arrays
extern const unsigned char epd_bitmap_Property_1_BPM[];
extern const unsigned char epd_bitmap_Property_1_Minus[];
extern const unsigned char epd_bitmap_Property_1_Metronome[];
extern const unsigned char epd_bitmap_Property_1_Measure[];
extern const unsigned char epd_bitmap_Property_1_Volume_33[];
extern const unsigned char epd_bitmap_Property_1_Volume[];
extern const unsigned char epd_bitmap_Property_1_Plus[];
extern const unsigned char epd_bitmap_Property_1_Volume_66[];
extern const unsigned char epd_bitmap_Property_1_Bar[];
extern const unsigned char epd_bitmap_Property_1_Battery_50[];
extern const unsigned char epd_bitmap_Property_1_Battery_0[];
extern const unsigned char epd_bitmap_Property_1_Battery_75[];
extern const unsigned char epd_bitmap_Property_1_Battery_25[];
extern const unsigned char epd_bitmap_Property_1_Beat[];
extern const unsigned char epd_bitmap_Property_1_Battery_100[];
extern const unsigned char epd_bitmap_Property_1_Increase[];
extern const unsigned char epd_bitmap_Property_1_Increase_off[];

extern const unsigned char image_Property_1_Battery_100_bits[];
extern const unsigned char image_Property_1_Volume_bits[];

// Declare the bitmap array
extern const unsigned char *epd_bitmap_allArray[];
extern const int epd_bitmap_allArray_LEN;

#endif // BITMAPS_H
