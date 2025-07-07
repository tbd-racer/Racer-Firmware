#include <hardware/pio.h>
#include <pico/time.h>
#include <pico/types.h>
#include <stdio.h>
#include <string.h>

#include "driver/rfm9x.h"
#include "driver/ws2812.h"
#include "pico/binary_info.h"
#include "pico/rand.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"

bi_decl(bi_3pins_with_func(RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_SCK_PIN, GPIO_FUNC_SPI));
bi_decl(bi_1pin_with_name(RADIO_CS_PIN, "RADIO CS"));
bi_decl(bi_1pin_with_name(BUTTON_LED_PIN, "Kill LED"));
bi_decl(bi_1pin_with_name(BUTTON_STAT_PIN, "Kill BTN"));

rfm9x_t radio;
union ws2812_command commands[BUTTON_NUM_LEDS];

absolute_time_t next_kill_state_change;
bool kill_state_toggle = true;

void toggleKillState() {
    kill_state_toggle = !kill_state_toggle;
    next_kill_state_change = make_timeout_time_ms(300);

    if (kill_state_toggle) {
        printf("Asserting kill\n");

        commands[0].data = 0u;
        commands[0].cmd.red = 50u;
        ws2812_strip_set(commands);
    } else {
        printf("Clearing kill\n");

        commands[0].data = 0u;
        commands[0].cmd.green = 50u;
        ws2812_strip_set(commands);
    }
}

int main() {
    // Setup the button LED
    ws2812_init(pio0, 0, BUTTON_LED_PIN, BUTTON_NUM_LEDS);

    // Initialize stdio
    stdio_init_all();

    // wait for usb serial to be ready
    while (!stdio_usb_connected() && to_ms_since_boot(get_absolute_time()) < 10000) {
        printf("Waiting for serial...\n");

        commands[0].data = 0u;
        commands[0].cmd.red = 50u;
        commands[0].cmd.green = 50u;
        ws2812_strip_set(commands);

        sleep_ms(250);

        commands[0].data = 0u;
        ws2812_strip_set(commands);

        sleep_ms(250);
    }

    printf("Initializing radio\n");

    // setup hardware spi 0
    gpio_set_function(RADIO_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RADIO_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RADIO_SCK_PIN, GPIO_FUNC_SPI);
    spi_init(spi0, RADIO_SPI_BAUDRATE);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // setup button pin
    gpio_init(BUTTON_STAT_PIN);
    gpio_set_dir(BUTTON_STAT_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_STAT_PIN);

    // Initialize RFM95 radio
    if (!rfm9x_init(&radio, spi0, RADIO_CS_PIN, RADIO_RST_PIN, RADIO_FREQUENCY)) {
        panic("failed radio init");
    }

    // Configure radio parameters
    rfm9x_set_spreading_factor(&radio, RADIO_SPREADING_FACTOR);
    rfm9x_set_signal_bandwidth(&radio, RADIO_SIGNAL_BANDWIDTH);
    rfm9x_set_coding_rate(&radio, RADIO_CODING_RATE);

    printf("Radio init complete\n");

    // Enter main loop
    uint32_t count = 0;
    uint8_t message[2];

    // force toggle the kill to de-assert
    toggleKillState();

    while (true) {
        // Read button state and create appropriate message
        bool button_pressed = gpio_get(BUTTON_STAT_PIN);
        if (button_pressed && get_absolute_time() > next_kill_state_change) {
            toggleKillState();
        }

        message[0] = 6;                           // ID byte
        message[1] = (uint8_t)kill_state_toggle;  // status

        // Send the message
        rfm9x_send(&radio, message, 2, false);

        sleep_ms(10);  // Wait 10ms before sending the next packet
        count++;
    }

    return 0;
}
