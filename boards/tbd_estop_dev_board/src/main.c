#include "pico/stdlib.h"
#include <pico/binary_info.h>
#include <pico/rand.h>
#include <pico/stdio.h>
#include <rfm95/rfm9x.h>
#include <stdio.h>

#define PICO_DEFAULT_LED_PIN 25

static const uint32_t spi_rx_pin = 16;
static const uint32_t spi_tx_pin = 19;
static const uint32_t spi_sck_pin = 18;
static const uint32_t spi_cs_pin = 17;
static const uint32_t irq_pin = 15;
static const uint32_t radio_rst_pin = 14;

static bool handle_rfm95_interrupt = false;

bi_decl(bi_3pins_with_func(spi_rx_pin, spi_tx_pin, spi_sck_pin, GPIO_FUNC_SPI));

void gpio_callback(uint gpio, uint32_t events) {
  (void)events;
  if (gpio == irq_pin) {
    handle_rfm95_interrupt = true;
  }
}

int pico_led_init(void) {
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  return PICO_OK;
}

void pico_set_led(bool led_on) { gpio_put(PICO_DEFAULT_LED_PIN, led_on); }

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
  gpio_put(spi_cs_pin, true);
  bi_decl(bi_1pin_with_name(spi_cs_pin, "SPI CS"));

  // setup the RFM95 interrupt pin
  gpio_init(irq_pin);
  gpio_set_dir(irq_pin, GPIO_IN);
  gpio_set_irq_enabled_with_callback(irq_pin, GPIO_IRQ_EDGE_RISE, true,
                                     gpio_callback);

  // Initialize stdio
  stdio_init_all();

  // wait for usb serial to be ready
  while (!stdio_usb_connected()) {
    pico_set_led(true);
    sleep_ms(250);
    pico_set_led(false);
    sleep_ms(250);
  }

  printf("RFM9x Receiver Demo\n");

  // Initialize RFM9x radio with correct parameters
  rfm9x_t radio;
  if (!rfm9x_init(&radio, spi0, spi_cs_pin, radio_rst_pin, 915000000)) {
    printf("Radio initialization failed!\n");
    return -1;
  }

  // Configure radio settings
  rfm9x_set_spreading_factor(&radio, 8);
  rfm9x_set_signal_bandwidth(&radio, 125000);
  rfm9x_set_coding_rate(&radio, 5);

  printf("Radio initialized - listening for packets...\n");
  printf("Frequency: %.1f MHz\n", rfm9x_get_frequency_mhz(&radio));
  printf("Spreading Factor: %d\n", rfm9x_get_spreading_factor(&radio));
  printf("Bandwidth: %lu Hz\n", rfm9x_get_signal_bandwidth(&radio));
  printf("Coding Rate: 4/%d\n", rfm9x_get_coding_rate(&radio));

  // Start listening
  rfm9x_listen(&radio);

  while (true) {
    // Handle interrupt-driven reception
    if (handle_rfm95_interrupt) {
      printf("RFM95 Interrupt triggered\n");
      handle_rfm95_interrupt = false;
    }

    // Check for received packets
    uint8_t packet_buffer[256];
    int received = rfm9x_receive(&radio, packet_buffer, sizeof(packet_buffer),
                                 true, false, false, 100);

    if (received > 0) {
      printf("Received %d bytes: ", received);

      // Print as hex
      for (int i = 0; i < received; ++i) {
        printf("%02X ", packet_buffer[i]);
      }
      printf("\n");

      // Try to print as string if printable
      printf("As string: \"");
      for (int i = 0; i < received; ++i) {
        if (packet_buffer[i] >= 32 && packet_buffer[i] < 127) {
          printf("%c", packet_buffer[i]);
        } else {
          printf(".");
        }
      }
      printf("\"\n");

      printf("RSSI: %.1f dBm, SNR: %.1f dB\n", radio.last_rssi, radio.last_snr);
      printf("---\n");

      // Blink LED on packet reception
      pico_set_led(true);
      sleep_ms(50);
      pico_set_led(false);
    }

    sleep_ms(10);
  }

  return 0;
}
