#ifndef DRIVER__WS2812_H_
#define DRIVER__WS2812_H_

#include "hardware/pio.h"

// Union for RGB pixels
#ifdef PIXEL_IS_RGBW
union ws2812_command {
    uint32_t data;
    struct __attribute__((__packed__)) {
        uint8_t white;
        uint8_t blue;
        uint8_t red;
        uint8_t green;
    } cmd;
};
#else
union ws2812_command {
    uint32_t data;
    struct __attribute__((__packed__)) {
        uint8_t reserved;
        uint8_t blue;
        uint8_t red;
        uint8_t green;
    } cmd;
};
#endif

/**
 * @brief Initializes the neopixel status LED strip library.
 * The strip will briefly flash a startup sequence after this is called.
 *
 * @param pio PIO machine to assign
 * @param sm PIO state machine to assign
 * @param pin Pin connected to pixels
 * @param num_leds number of pixels in the strip
 */
void ws2812_init(PIO pio, uint sm, uint pin, uint num_leds);


/**
 * @brief Sets the status strip to the following color command
 *
 * @param commands The LED strip commands per pixel
 */
void ws2812_strip_set(union ws2812_command commands[]);



#endif