#include "pico/stdlib.h"
#include <pico/binary_info.h>
#include <pico/rand.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <rfm95/rfm9x.h>
#include <stdio.h>
#include <string.h>

#define RADIO_FREQ_MHZ 915.0

const uint32_t spi_rx_pin = 20;  // SPI RX pin
const uint32_t spi_tx_pin = 19;  // SPI TX pin
const uint32_t spi_sck_pin = 18; // SPI SCK pin
const uint32_t spi_cs_pin = 8;   // SPI CS pin
const uint32_t reset_pin = 9;    // RFM95 reset pin
const uint32_t button_pin = 25;  // Button pin

bi_decl(bi_3pins_with_func(spi_rx_pin, spi_tx_pin, spi_sck_pin, GPIO_FUNC_SPI));

rfm9x_t radio;

int main() {

  // setup hardware spi 0
  spi_init(spi0, 2000 * 2000);
  spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_set_function(spi_rx_pin, GPIO_FUNC_SPI);
  gpio_set_function(spi_tx_pin, GPIO_FUNC_SPI);
  gpio_set_function(spi_sck_pin, GPIO_FUNC_SPI);

  // setup chip select pin
  gpio_init(spi_cs_pin);
  gpio_set_dir(spi_cs_pin, GPIO_OUT);
  gpio_put(spi_cs_pin, true); // Set CS pin high
  bi_decl(bi_1pin_with_name(spi_cs_pin, "SPI CS"));

  // setup button pin
  gpio_init(button_pin);
  gpio_set_dir(button_pin, GPIO_IN);
  gpio_pull_up(button_pin);

  // Initialize RFM95 radio
  if (!rfm9x_init(&radio, spi0, spi_cs_pin, reset_pin,
                  RADIO_FREQ_MHZ * 1000000)) {
    return -1;
  }

  // Configure radio parameters to match Python example
  rfm9x_set_spreading_factor(&radio, 8);      // SF8
  rfm9x_set_signal_bandwidth(&radio, 125000); // 125kHz

  // Enter main loop
  uint32_t count = 0;
  uint8_t message[2];

  while (true) {
    // Read button state and create appropriate message
    bool button_pressed = gpio_get(button_pin);

    message[0] = 6; // ID byte
    if (button_pressed) {
      message[1] = 1; // stopped
    } else {
      message[1] = 0; // go
    }

    // Send the message
    rfm9x_send(&radio, message, 2, false);

    sleep_ms(10); // Wait 10ms before sending the next packet
    count++;
  }

  return 0;
}
