#include "driver/rfm9x.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <stdlib.h>
#include <string.h>

// Bandwidth lookup table
static const uint32_t bw_bins[] = {7800,  10400, 15600,  20800,  31250,
                                   41700, 62500, 125000, 250000, 500000};

// Helper function to get register bits
static uint8_t get_register_bits(rfm9x_t *rfm, uint8_t address, uint8_t offset,
                                 uint8_t bits) {
  uint8_t mask = ((1 << bits) - 1) << offset;
  uint8_t reg_value = rfm9x_read_u8(rfm, address);
  return (reg_value & mask) >> offset;
}

// Helper function to set register bits
static void set_register_bits(rfm9x_t *rfm, uint8_t address, uint8_t offset,
                              uint8_t bits, uint8_t value) {
  uint8_t mask = ((1 << bits) - 1) << offset;
  uint8_t reg_value = rfm9x_read_u8(rfm, address);
  reg_value = (reg_value & ~mask) | ((value << offset) & mask);
  rfm9x_write_u8(rfm, address, reg_value);
}

uint8_t rfm9x_read_u8(rfm9x_t *rfm, uint8_t address) {
  uint8_t tx_data = address & 0x7F; // Clear MSB for read
  uint8_t rx_data;

  gpio_put(rfm->cs_pin, 0);
  spi_write_blocking(rfm->spi, &tx_data, 1);
  spi_read_blocking(rfm->spi, 0, &rx_data, 1);
  gpio_put(rfm->cs_pin, 1);

  return rx_data;
}

void rfm9x_write_u8(rfm9x_t *rfm, uint8_t address, uint8_t value) {
  uint8_t tx_data[2] = {address | 0x80, value}; // Set MSB for write

  gpio_put(rfm->cs_pin, 0);
  spi_write_blocking(rfm->spi, tx_data, 2);
  gpio_put(rfm->cs_pin, 1);
}

void rfm9x_read_into(rfm9x_t *rfm, uint8_t address, uint8_t *buffer,
                     size_t length) {
  uint8_t tx_data = address & 0x7F; // Clear MSB for read

  gpio_put(rfm->cs_pin, 0);
  spi_write_blocking(rfm->spi, &tx_data, 1);
  spi_read_blocking(rfm->spi, 0, buffer, length);
  gpio_put(rfm->cs_pin, 1);
}

void rfm9x_write_from(rfm9x_t *rfm, uint8_t address, const uint8_t *buffer,
                      size_t length) {
  uint8_t tx_data = address | 0x80; // Set MSB for write

  gpio_put(rfm->cs_pin, 0);
  spi_write_blocking(rfm->spi, &tx_data, 1);
  spi_write_blocking(rfm->spi, buffer, length);
  gpio_put(rfm->cs_pin, 1);
}

void rfm9x_reset(rfm9x_t *rfm) {
  gpio_put(rfm->reset_pin, 0);
  sleep_us(100);
  gpio_put(rfm->reset_pin, 1);
  sleep_ms(5);
}

bool rfm9x_init(rfm9x_t *rfm, spi_inst_t *spi, uint cs_pin, uint reset_pin,
                uint32_t frequency_hz) {
  memset(rfm, 0, sizeof(rfm9x_t));

  rfm->spi = spi;
  rfm->cs_pin = cs_pin;
  rfm->reset_pin = reset_pin;
  rfm->frequency_hz = frequency_hz;
  rfm->preamble_length = 8;
  rfm->high_power = true;
  rfm->baudrate = 5000000;
  rfm->auto_agc = false;
  rfm->enable_crc = true;

  // Initialize default values
  rfm->node = RH_BROADCAST_ADDRESS;
  rfm->destination = RH_BROADCAST_ADDRESS;
  rfm->identifier = 0;
  rfm->flags = 0;
  rfm->sequence_number = 0;
  rfm->ack_wait = 0.5;
  rfm->receive_timeout = 0.5;
  rfm->xmit_timeout = 2.0;
  rfm->ack_retries = 5;
  rfm->ack_delay = 0.0;

  // Initialize GPIO
  gpio_init(cs_pin);
  gpio_set_dir(cs_pin, GPIO_OUT);
  gpio_put(cs_pin, 1);

  gpio_init(reset_pin);
  gpio_set_dir(reset_pin, GPIO_OUT);
  gpio_put(reset_pin, 1);

  // Reset the device
  rfm9x_reset(rfm);

  // Check version
  uint8_t version = rfm9x_read_u8(rfm, RH_RF95_REG_42_VERSION);
  if (version != 18) {
    return false;
  }

  // Set sleep mode and enable LoRa mode
  rfm9x_sleep(rfm);
  sleep_ms(10);
  set_register_bits(rfm, RH_RF95_REG_01_OP_MODE, 7, 1, 1); // Long range mode

  // Check if we're in sleep mode and LoRa mode
  uint8_t op_mode = get_register_bits(rfm, RH_RF95_REG_01_OP_MODE, 0, 3);
  bool long_range = get_register_bits(rfm, RH_RF95_REG_01_OP_MODE, 7, 1);
  if (op_mode != SLEEP_MODE || !long_range) {
    return false;
  }

  // Clear low frequency mode for frequencies > 525MHz
  if (frequency_hz > 525000000) {
    set_register_bits(rfm, RH_RF95_REG_01_OP_MODE, 3, 1, 0);
  }

  // Setup FIFO
  rfm9x_write_u8(rfm, RH_RF95_REG_0E_FIFO_TX_BASE_ADDR, 0x00);
  rfm9x_write_u8(rfm, RH_RF95_REG_0F_FIFO_RX_BASE_ADDR, 0x00);

  // Set to idle mode
  rfm9x_idle(rfm);

  // Set frequency
  rfm9x_set_frequency_mhz(rfm, frequency_hz / 1000000.0);

  // Set default parameters
  rfm9x_set_signal_bandwidth(rfm, 125000);
  rfm9x_set_coding_rate(rfm, 5);
  rfm9x_set_spreading_factor(rfm, 7);
  rfm9x_set_tx_power(rfm, 13);

  return true;
}

void rfm9x_sleep(rfm9x_t *rfm) {
  set_register_bits(rfm, RH_RF95_REG_01_OP_MODE, 0, 3, SLEEP_MODE);
}

void rfm9x_idle(rfm9x_t *rfm) {
  set_register_bits(rfm, RH_RF95_REG_01_OP_MODE, 0, 3, STANDBY_MODE);
}

void rfm9x_listen(rfm9x_t *rfm) {
  set_register_bits(rfm, RH_RF95_REG_01_OP_MODE, 0, 3, RX_MODE);
  set_register_bits(rfm, RH_RF95_REG_40_DIO_MAPPING1, 6, 2,
                    0b00); // Interrupt on rx done
}

void rfm9x_transmit(rfm9x_t *rfm) {
  set_register_bits(rfm, RH_RF95_REG_01_OP_MODE, 0, 3, TX_MODE);
  set_register_bits(rfm, RH_RF95_REG_40_DIO_MAPPING1, 6, 2,
                    0b01); // Interrupt on tx done
}

void rfm9x_set_frequency_mhz(rfm9x_t *rfm, float frequency_mhz) {
  if (frequency_mhz < 240.0 || frequency_mhz > 960.0) {
    return; // Invalid frequency
  }

  uint32_t frf =
      (uint32_t)((frequency_mhz * 1000000.0) / RH_RF95_FSTEP) & 0xFFFFFF;

  rfm9x_write_u8(rfm, RH_RF95_REG_06_FRF_MSB, (frf >> 16) & 0xFF);
  rfm9x_write_u8(rfm, RH_RF95_REG_07_FRF_MID, (frf >> 8) & 0xFF);
  rfm9x_write_u8(rfm, RH_RF95_REG_08_FRF_LSB, frf & 0xFF);
}

float rfm9x_get_frequency_mhz(rfm9x_t *rfm) {
  uint32_t frf = (rfm9x_read_u8(rfm, RH_RF95_REG_06_FRF_MSB) << 16) |
                 (rfm9x_read_u8(rfm, RH_RF95_REG_07_FRF_MID) << 8) |
                 rfm9x_read_u8(rfm, RH_RF95_REG_08_FRF_LSB);

  return (frf * RH_RF95_FSTEP) / 1000000.0;
}

void rfm9x_set_tx_power(rfm9x_t *rfm, int8_t power) {
  if (rfm->high_power) {
    if (power < 5 || power > 23)
      return;

    if (power > 20) {
      set_register_bits(rfm, RH_RF95_REG_4D_PA_DAC, 0, 3,
                        RH_RF95_PA_DAC_ENABLE);
      power -= 3;
    } else {
      set_register_bits(rfm, RH_RF95_REG_4D_PA_DAC, 0, 3,
                        RH_RF95_PA_DAC_DISABLE);
    }

    set_register_bits(rfm, RH_RF95_REG_09_PA_CONFIG, 7, 1, 1); // PA_SELECT
    set_register_bits(rfm, RH_RF95_REG_09_PA_CONFIG, 0, 4, (power - 5) & 0x0F);
  } else {
    if (power < -1 || power > 14)
      return;

    set_register_bits(rfm, RH_RF95_REG_09_PA_CONFIG, 7, 1, 0);     // PA_SELECT
    set_register_bits(rfm, RH_RF95_REG_09_PA_CONFIG, 4, 3, 0b111); // MAX_POWER
    set_register_bits(rfm, RH_RF95_REG_09_PA_CONFIG, 0, 4, (power + 1) & 0x0F);
  }
}

void rfm9x_set_signal_bandwidth(rfm9x_t *rfm, uint32_t bandwidth) {
  uint8_t bw_id = 9; // Default to highest bandwidth

  for (int i = 0; i < 9; i++) {
    if (bandwidth <= bw_bins[i]) {
      bw_id = i;
      break;
    }
  }

  set_register_bits(rfm, RH_RF95_REG_1D_MODEM_CONFIG1, 4, 4, bw_id);

  // Handle errata notes
  if (bandwidth >= 500000) {
    set_register_bits(rfm, RH_RF95_DETECTION_OPTIMIZE, 7, 1, 1); // AUTO_IFON
    bool low_freq = get_register_bits(rfm, RH_RF95_REG_01_OP_MODE, 3, 1);

    rfm9x_write_u8(rfm, 0x36, 0x02);
    rfm9x_write_u8(rfm, 0x3A, low_freq ? 0x7F : 0x64);
  } else {
    set_register_bits(rfm, RH_RF95_DETECTION_OPTIMIZE, 7, 1, 0); // AUTO_IFON
    rfm9x_write_u8(rfm, 0x36, 0x03);

    if (bandwidth == 7800) {
      rfm9x_write_u8(rfm, 0x2F, 0x48);
    } else if (bandwidth >= 62500) {
      rfm9x_write_u8(rfm, 0x2F, 0x40);
    } else {
      rfm9x_write_u8(rfm, 0x2F, 0x44);
    }
    rfm9x_write_u8(rfm, 0x30, 0);
  }
}

void rfm9x_set_coding_rate(rfm9x_t *rfm, uint8_t rate) {
  if (rate < 5 || rate > 8)
    return;

  uint8_t cr_id = rate - 4;
  set_register_bits(rfm, RH_RF95_REG_1D_MODEM_CONFIG1, 1, 3, cr_id);
}

void rfm9x_set_spreading_factor(rfm9x_t *rfm, uint8_t factor) {
  if (factor < 6 || factor > 12)
    return;

  if (factor == 6) {
    set_register_bits(rfm, RH_RF95_DETECTION_OPTIMIZE, 0, 3, 0x5);
    rfm9x_write_u8(rfm, RH_RF95_DETECTION_THRESHOLD, 0x0C);
  } else {
    set_register_bits(rfm, RH_RF95_DETECTION_OPTIMIZE, 0, 3, 0x3);
    rfm9x_write_u8(rfm, RH_RF95_DETECTION_THRESHOLD, 0x0A);
  }

  set_register_bits(rfm, RH_RF95_REG_1E_MODEM_CONFIG2, 4, 4, factor);
}

uint8_t rfm9x_get_spreading_factor(rfm9x_t *rfm) {
  return get_register_bits(rfm, RH_RF95_REG_1E_MODEM_CONFIG2, 4, 4);
}

uint32_t rfm9x_get_signal_bandwidth(rfm9x_t *rfm) {
  uint8_t bw_id = get_register_bits(rfm, RH_RF95_REG_1D_MODEM_CONFIG1, 4, 4);
  if (bw_id >= 9) {
    return 500000;
  }
  return bw_bins[bw_id];
}

uint8_t rfm9x_get_coding_rate(rfm9x_t *rfm) {
  uint8_t cr_id = get_register_bits(rfm, RH_RF95_REG_1D_MODEM_CONFIG1, 1, 3);
  return cr_id + 4;
}

int8_t rfm9x_get_tx_power(rfm9x_t *rfm) {
  if (rfm->high_power) {
    return get_register_bits(rfm, RH_RF95_REG_09_PA_CONFIG, 0, 4) + 5;
  } else {
    return get_register_bits(rfm, RH_RF95_REG_09_PA_CONFIG, 0, 4) - 1;
  }
}

bool rfm9x_tx_done(rfm9x_t *rfm) {
  return (rfm9x_read_u8(rfm, RH_RF95_REG_12_IRQ_FLAGS) & 0x08) != 0;
}

bool rfm9x_rx_done(rfm9x_t *rfm) {
  return (rfm9x_read_u8(rfm, RH_RF95_REG_12_IRQ_FLAGS) & 0x40) != 0;
}

bool rfm9x_crc_error(rfm9x_t *rfm) {
  return (rfm9x_read_u8(rfm, RH_RF95_REG_12_IRQ_FLAGS) & 0x20) != 0;
}

int rfm9x_get_rssi(rfm9x_t *rfm) {
  int raw_rssi = rfm9x_read_u8(rfm, RH_RF95_REG_1A_PKT_RSSI_VALUE);
  bool low_freq = get_register_bits(rfm, RH_RF95_REG_01_OP_MODE, 3, 1);

  return low_freq ? (raw_rssi - 157) : (raw_rssi - 164);
}

float rfm9x_get_snr(rfm9x_t *rfm) {
  int snr_byte = rfm9x_read_u8(rfm, RH_RF95_REG_19_PKT_SNR_VALUE);
  if (snr_byte > 127) {
    snr_byte = (256 - snr_byte) * -1;
  }
  return snr_byte / 4.0;
}

bool rfm9x_send(rfm9x_t *rfm, const uint8_t *data, size_t length,
                bool keep_listening) {
  if (length == 0 || length > 252)
    return false;

  rfm9x_idle(rfm);

  // Prepare payload with header
  uint8_t payload[256];
  payload[0] = rfm->destination;
  payload[1] = rfm->node;
  payload[2] = rfm->identifier;
  payload[3] = rfm->flags;
  memcpy(&payload[4], data, length);
  size_t payload_length = length + 4;

  // Write to FIFO
  rfm9x_write_u8(rfm, RH_RF95_REG_0D_FIFO_ADDR_PTR, 0x00);
  rfm9x_write_from(rfm, RH_RF95_REG_00_FIFO, payload, payload_length);
  rfm9x_write_u8(rfm, RH_RF95_REG_22_PAYLOAD_LENGTH, payload_length);

  // Transmit
  rfm9x_transmit(rfm);

  // Wait for completion
  absolute_time_t timeout =
      make_timeout_time_ms((uint32_t)(rfm->xmit_timeout * 1000));
  while (!rfm9x_tx_done(rfm) && !time_reached(timeout)) {
    tight_loop_contents();
  }

  bool success = rfm9x_tx_done(rfm);

  if (keep_listening) {
    rfm9x_listen(rfm);
  } else {
    rfm9x_idle(rfm);
  }

  // Clear interrupts
  rfm9x_write_u8(rfm, RH_RF95_REG_12_IRQ_FLAGS, 0xFF);

  return success;
}

int rfm9x_receive(rfm9x_t *rfm, uint8_t *buffer, size_t buffer_size,
                  bool keep_listening, bool with_header, bool with_ack,
                  uint32_t timeout_ms) {

  rfm9x_listen(rfm);

  // Wait for packet
  absolute_time_t timeout = make_timeout_time_ms(timeout_ms);
  while (!rfm9x_rx_done(rfm) && !time_reached(timeout)) {
    tight_loop_contents();
  }

  if (!rfm9x_rx_done(rfm)) {
    if (keep_listening) {
      rfm9x_listen(rfm);
    } else {
      rfm9x_idle(rfm);
    }
    return -1; // Timeout
  }

  // Save RSSI and SNR
  rfm->last_rssi = rfm9x_get_rssi(rfm);
  rfm->last_snr = rfm9x_get_snr(rfm);

  rfm9x_idle(rfm);

  // Check for CRC error
  if (rfm->enable_crc && rfm9x_crc_error(rfm)) {
    rfm->crc_error_count++;
    rfm9x_write_u8(rfm, RH_RF95_REG_12_IRQ_FLAGS, 0xFF);
    if (keep_listening) {
      rfm9x_listen(rfm);
    }
    return -2; // CRC error
  }

  // Read packet
  uint8_t fifo_length = rfm9x_read_u8(rfm, RH_RF95_REG_13_RX_NB_BYTES);
  if (fifo_length < 5) {
    rfm9x_write_u8(rfm, RH_RF95_REG_12_IRQ_FLAGS, 0xFF);
    if (keep_listening) {
      rfm9x_listen(rfm);
    }
    return -3; // Packet too short
  }

  uint8_t current_addr =
      rfm9x_read_u8(rfm, RH_RF95_REG_10_FIFO_RX_CURRENT_ADDR);
  rfm9x_write_u8(rfm, RH_RF95_REG_0D_FIFO_ADDR_PTR, current_addr);

  uint8_t packet[256];
  rfm9x_read_into(rfm, RH_RF95_REG_00_FIFO, packet, fifo_length);

  // Clear interrupts
  rfm9x_write_u8(rfm, RH_RF95_REG_12_IRQ_FLAGS, 0xFF);

  // Check destination
  if (rfm->node != RH_BROADCAST_ADDRESS && packet[0] != RH_BROADCAST_ADDRESS &&
      packet[0] != rfm->node) {
    if (keep_listening) {
      rfm9x_listen(rfm);
    }
    return -4; // Not for us
  }

  // Handle ACK if requested
  if (with_ack && !(packet[3] & RH_FLAGS_ACK) &&
      packet[0] != RH_BROADCAST_ADDRESS) {
    if (rfm->ack_delay > 0) {
      sleep_ms((uint32_t)(rfm->ack_delay * 1000));
    }

    // Send ACK
    uint8_t old_dest = rfm->destination;
    uint8_t old_node = rfm->node;
    uint8_t old_id = rfm->identifier;
    uint8_t old_flags = rfm->flags;

    rfm->destination = packet[1];
    rfm->node = packet[0];
    rfm->identifier = packet[2];
    rfm->flags = packet[3] | RH_FLAGS_ACK;

    const uint8_t ack_data = '!';
    rfm9x_send(rfm, &ack_data, 1, false);

    rfm->destination = old_dest;
    rfm->node = old_node;
    rfm->identifier = old_id;
    rfm->flags = old_flags;

    // Check for retry
    if ((rfm->seen_ids[packet[1]] == packet[2]) &&
        (packet[3] & RH_FLAGS_RETRY)) {
      if (keep_listening) {
        rfm9x_listen(rfm);
      }
      return -5; // Duplicate packet
    }
    rfm->seen_ids[packet[1]] = packet[2];
  }

  // Copy data to buffer
  size_t data_start = with_header ? 0 : 4;
  size_t copy_length = fifo_length - data_start;
  if (copy_length > buffer_size) {
    copy_length = buffer_size;
  }

  memcpy(buffer, &packet[data_start], copy_length);

  if (keep_listening) {
    rfm9x_listen(rfm);
  }

  return copy_length;
}

// Additional helper functions for simplified C interface