#include <driver/rfm9x.h>
#include <pico/binary_info.h>
#include <pico/rand.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

bi_decl(bi_3pins_with_func(RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_SCK_PIN, GPIO_FUNC_SPI));

rfm9x_t radio;

int main() {
    // setup hardware spi 0
    spi_init(spi0, RADIO_SPI_BAUDRATE);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(RADIO_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RADIO_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RADIO_SCK_PIN, GPIO_FUNC_SPI);

    // setup chip select pin
    gpio_init(RADIO_CS_PIN);
    gpio_set_dir(RADIO_CS_PIN, GPIO_OUT);
    gpio_put(RADIO_CS_PIN, true);  // Set CS pin high
    bi_decl(bi_1pin_with_name(RADIO_CS_PIN, "SPI CS"));

    // setup button pin
    gpio_init(BUTTON_STAT_PIN);
    gpio_set_dir(BUTTON_STAT_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_STAT_PIN);

    // Initialize RFM95 radio
    if (!rfm9x_init(&radio, spi0, RADIO_CS_PIN, RADIO_RST_PIN, RADIO_FREQUENCY)) {
        return -1;
    }

    // Configure radio parameters
    rfm9x_set_spreading_factor(&radio, RADIO_SPREADING_FACTOR);
    rfm9x_set_signal_bandwidth(&radio, RADIO_SIGNAL_BANDWIDTH);
    rfm9x_set_coding_rate(&radio, RADIO_CODING_RATE);

    // Enter main loop
    uint32_t count = 0;
    uint8_t message[2];

    while (true) {
        // Read button state and create appropriate message
        bool button_pressed = gpio_get(BUTTON_STAT_PIN);

        message[0] = 6;  // ID byte
        if (button_pressed) {
            message[1] = 1;  // stopped
        } else {
            message[1] = 0;  // go
        }

        // Send the message
        rfm9x_send(&radio, message, 2, false);

        sleep_ms(10);  // Wait 10ms before sending the next packet
        count++;
    }

    return 0;
}
