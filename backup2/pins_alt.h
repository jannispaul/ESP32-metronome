#pragma once
#include <stdint.h>

namespace Pins {
    constexpr uint8_t EncoderCLK = 17;
    constexpr uint8_t EncoderDT  = 16;
    constexpr uint8_t EncoderBTN = 4;

    constexpr uint8_t Button1 = 32;
    constexpr uint8_t Button2 = 33;
    constexpr uint8_t Button3 = 13;

    constexpr uint8_t LED        = 2;

    // Audio AMP
    constexpr uint8_t I2S_DOUT   = 25;
    constexpr uint8_t I2S_BCLK   = 26;
    constexpr uint8_t I2S_LRC    = 27;

    // Display
    constexpr uint8_t I2C_SDA    = 21;
    constexpr uint8_t I2C_SCL    = 22;
}
