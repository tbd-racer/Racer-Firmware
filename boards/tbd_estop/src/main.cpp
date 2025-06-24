#include "pico/stdlib.h"
#include <pico/binary_info.h>
#include <pico/rand.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <rfm95/rfm95.hpp>
#include <stdio.h>
#include <string>

constexpr uint32_t spi_rx_pin = 16;  // SPI RX pin
constexpr uint32_t spi_tx_pin = 19;  // SPI TX pin
constexpr uint32_t spi_sck_pin = 18; // SPI SCK pin
constexpr uint32_t spi_cs_pin = 17;  // SPI CS pin

bi_decl(bi_3pins_with_func(spi_rx_pin, spi_tx_pin, spi_sck_pin, GPIO_FUNC_SPI));

#define PICO_DEFAULT_LED_PIN 25

// Perform initialisation
int pico_led_init(void) {
  // A device like Pico that uses a GPIO for the LED will define
  // PICO_DEFAULT_LED_PIN so we can use normal GPIO functionality to turn the
  // led on and off
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  return PICO_OK;
}

// Turn the led on or off
void pico_set_led(bool led_on) {
  // Just set the GPIO on or off
  gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

int main() {

  pico_led_init();

  spi_init(spi0, 1000 * 1000);
  gpio_set_function(spi_rx_pin, GPIO_FUNC_SPI);
  gpio_set_function(spi_tx_pin, GPIO_FUNC_SPI);
  gpio_set_function(spi_sck_pin, GPIO_FUNC_SPI);
  // gpio_set_function(spi_cs_pin, GPIO_FUNC_SPI);

  gpio_init(spi_cs_pin);
  gpio_set_dir(spi_cs_pin, GPIO_OUT);
  gpio_put(spi_cs_pin, true); // Set CS pin high
  bi_decl(bi_1pin_with_name(spi_cs_pin, "SPI CS"));

  // Initialize stdio
  // stdio_init_all();
  stdio_init_all();

  // wait for usb serial to be ready
  while (!stdio_usb_connected()) {
    pico_set_led(true);
    sleep_ms(250);
    pico_set_led(false);
    sleep_ms(250);
  }

  rfm95::RFM95 rfm95(spi0, spi_cs_pin);

  if (!rfm95.write_device_mode(rfm95::OpModes::STANDBY).has_value()) {
    printf("Failed to set device mode to SLEEP\n");
  }

  // Enter main loop
  while (true) {
    pico_set_led(true);
    sleep_ms(500);
    pico_set_led(false);
    sleep_ms(500);

    const auto mode =
        rfm95.read_device_mode()
            .transform([](uint8_t mode_value) {
              char buffer[32];
              snprintf(buffer, sizeof(buffer), "Device Mode: 0b%03u\n",
                       (mode_value & 0b111));
              return std::string(buffer);
            })
            .value_or("Error reading device mode\n");

    printf(mode.c_str());
  }

  return 0;
}