#include "pico/stdlib.h"
#include <pico/binary_info.h>
#include <pico/rand.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <rfm95/rfm95.hpp>
#include <stdio.h>
#include <string>

#define PICO_DEFAULT_LED_PIN 25

constexpr uint32_t spi_rx_pin = 16;  // SPI RX pin
constexpr uint32_t spi_tx_pin = 19;  // SPI TX pin
constexpr uint32_t spi_sck_pin = 18; // SPI SCK pin
constexpr uint32_t spi_cs_pin = 17;  // SPI CS pin
constexpr uint32_t irq_pin = 15;     // RFM95 interrupt pin

bi_decl(bi_3pins_with_func(spi_rx_pin, spi_tx_pin, spi_sck_pin, GPIO_FUNC_SPI));

rfm95::RFM95 radio(spi0, spi_cs_pin);

bool handle_rfm95_interrupt = false;

/// GPIO interrupt callback function
/// @param gpio GPIO pin number that triggered the interrupt
/// @param events Event mask that triggered the interrupt
void gpio_callback(uint gpio, uint32_t events) {
  if (gpio == 15) {
    /// Handle the RFM95 interrupt
    handle_rfm95_interrupt = true;
  }
}

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

  // // setup the RFM95 interrupt pin
  gpio_init(irq_pin);
  gpio_set_dir(irq_pin, GPIO_IN);
  gpio_set_irq_enabled_with_callback(irq_pin, GPIO_IRQ_EDGE_RISE, true,
                                     gpio_callback);

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

  radio.set_long_range_mode(rfm95::LongRangeMode::LoRa)
      .set_device_mode(rfm95::OpModes::STANDBY)
      .set_header_mode(rfm95::ImplicitHeaderMode::Explicit)
      .set_spreading_factor(rfm95::SpreadingFactors::SF12)
      .set_bandwidth(rfm95::SignalBandwidth::BW_20_8_kHz)
      .set_coding_rate(rfm95::CodingRate::CR4_8);

  // Enter main loop
  std::array<uint8_t, 2> data_to_send = {0x01, 0x02};

  while (true) {

    // interrupt has been triggered and we need to handle it
    // cannot do this in the interrupt handler because it uses
    // spi.
    if (handle_rfm95_interrupt) {
      handle_rfm95_interrupt = false;
      radio.interrupt_callback();
    }

    const auto mode = radio.read_device_mode();

    const auto transmit_state = radio.transmit(data_to_send);

    printf("Current mode: %d\n", static_cast<int>(mode));
  }

  return 0;
}