#include "pico/stdlib.h"
#include <pico/binary_info.h>
#include <pico/rand.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>
#include <rfm95/rfm95.hpp>
#include <stdio.h>
#include <string>

#define PICO_DEFAULT_LED_PIN 25

constexpr uint32_t spi_rx_pin = 16;    // SPI RX pin
constexpr uint32_t spi_tx_pin = 19;    // SPI TX pin
constexpr uint32_t spi_sck_pin = 18;   // SPI SCK pin
constexpr uint32_t spi_cs_pin = 17;    // SPI CS pin
constexpr uint32_t irq_pin = 15;       // RFM95 interrupt pin
constexpr uint32_t radio_rst_pin = 14; // RFM95 reset pin

// constexpr uint32_t spi_rx_pin = 20;   // SPI RX pin
// constexpr uint32_t spi_tx_pin = 19;   // SPI TX pin
// constexpr uint32_t spi_sck_pin = 18;  // SPI SCK pin
// constexpr uint32_t spi_cs_pin = 8;    // SPI CS pin
// constexpr uint32_t irq_pin = 7;       // RFM95 interrupt pin
// constexpr uint32_t radio_rst_pin = 9; // RFM95 reset pin

bi_decl(bi_3pins_with_func(spi_rx_pin, spi_tx_pin, spi_sck_pin, GPIO_FUNC_SPI));

bool handle_rfm95_interrupt = false;

/// GPIO interrupt callback function
/// @param gpio GPIO pin number that triggered the interrupt
/// @param events Event mask that triggered the interrupt
void gpio_callback(uint gpio, uint32_t events) {
  if (gpio == irq_pin) {
    /// Handle the RFM95 interrupt
    handle_rfm95_interrupt = true;
  }
}

std::array<uint8_t, 256> rx_fifo_buffer;
size_t rx_fifo_buffer_size = 0;
bool recieved = false;
void on_receive_callback(std::span<const uint8_t> data) {
  rx_fifo_buffer_size = data.size();
  std::copy(data.begin(), data.end(), rx_fifo_buffer.begin());
  recieved = true;
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

rfm95::RFM95 radio(spi0, spi_cs_pin, on_receive_callback);

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

  // reset the radio
  gpio_init(radio_rst_pin);
  gpio_set_dir(radio_rst_pin, GPIO_OUT);
  gpio_put(radio_rst_pin, true);
  sleep_ms(10); // Wait for 10 ms
  gpio_put(radio_rst_pin, false);
  sleep_us(100);
  gpio_put(radio_rst_pin, true);
  sleep_ms(10); // Wait for 10 ms

  radio.set_long_range_mode(rfm95::LongRangeMode::LoRa);
  sleep_ms(10);

  radio.set_device_mode(rfm95::OpModes::STANDBY);
  sleep_ms(10);

  radio.set_spreading_factor(rfm95::SpreadingFactors::SF8);
  sleep_ms(10);

  radio.set_bandwidth(rfm95::SignalBandwidth::BW_125_kHz);
  sleep_ms(10);

  radio.set_coding_rate(rfm95::CodingRate::CR4_5);
  sleep_ms(10);

  radio.set_frequency(915000000); // Set frequency to 915 MHz
  sleep_ms(10);

  radio.spi_.update_register(rfm95::RegModemConfig2::addr, [](uint8_t value) {
    // Set the LoRa modem to use implicit header mode
    return rfm95::RegModemConfig2::set_rx_payload_crc_on(
        value, rfm95::RxPayloadCrcOn::Off);
  });

  radio.enable_continuous_recieve();

  while (true) {

    // interrupt has been triggered and we need to handle it
    // cannot do this in the interrupt handler because it uses
    // spi.
    if (handle_rfm95_interrupt) {
      printf("RFM95 Interrupt triggered\n");
      handle_rfm95_interrupt = false;
      radio.interrupt_callback();
    }

    const auto mode = radio.read_device_mode();

    if (recieved) {
      printf("Received %zu bytes: ", rx_fifo_buffer_size);
      for (size_t i = 0; i < rx_fifo_buffer_size; ++i) {
        printf("%02X ", rx_fifo_buffer[i]);
      }
      printf("\n");
      recieved = false;
    }

    printf("Current mode: %d\n", static_cast<int>(mode));
    sleep_ms(250);
  }

  return 0;
}