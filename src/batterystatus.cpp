// Battery Status function from https://www.reddit.com/r/esp32/comments/1dybanl/comment/lc7gxma/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
// #include "esp_adc_cal.h" // So we can read the battery voltage

// uint32_t getBatteryVoltage(void)
// {
//     float v = 0.0;
//     v = (readADC_Cal(analogRead(BAT_ADC))) * 2;
//     CLOG(myLog1.add(), "getBatteryVoltage: %f", v);
//     return v;
// }

// /**
//  * @brief Get the battery voltage via the battery pin
//  *
//  * @param adc_raw Raw battery voltage from an adc read.
//  * @return uint32_t Battery voltage, e.g. 3999.0
//  */
// static uint32_t readADC_Cal(const int adc_raw)
// {
//     esp_adc_cal_characteristics_t adc_chars;
//     esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
//     return (esp_adc_cal_raw_to_voltage(adc_raw, &adc_chars));
// }

// /**
//  * @brief Calculate the appromimate battery life percentage remaining. Returns a value
//  * between 0-100% rounded to the nearest integer.
//  *
//  * @param v Voltage reading of the battery.
//  * @return int Percentage remaining
//  */

// int calculateBatteryPercentage(double v)
// {
//     // this formula was calculated using samples collected from a lipo battery
//     double y = -144.9390 * v * v * v + 1655.8629 * v * v - 6158.8520 * v + 7501.3202;

//     // enforce bounds, 0-100
//     y = max(y, 0.0);

//     y = min(y, 100.0);

//     y = round(y);

//     return static_cast<int>(y);
// }