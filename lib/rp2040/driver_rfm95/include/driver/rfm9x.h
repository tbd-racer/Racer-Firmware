#ifndef RFM9X_H
#define RFM9X_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/spi.h"

// Internal constants - Register names
#define RH_RF95_REG_00_FIFO 0x00
#define RH_RF95_REG_01_OP_MODE 0x01
#define RH_RF95_REG_06_FRF_MSB 0x06
#define RH_RF95_REG_07_FRF_MID 0x07
#define RH_RF95_REG_08_FRF_LSB 0x08
#define RH_RF95_REG_09_PA_CONFIG 0x09
#define RH_RF95_REG_0A_PA_RAMP 0x0A
#define RH_RF95_REG_0B_OCP 0x0B
#define RH_RF95_REG_0C_LNA 0x0C
#define RH_RF95_REG_0D_FIFO_ADDR_PTR 0x0D
#define RH_RF95_REG_0E_FIFO_TX_BASE_ADDR 0x0E
#define RH_RF95_REG_0F_FIFO_RX_BASE_ADDR 0x0F
#define RH_RF95_REG_10_FIFO_RX_CURRENT_ADDR 0x10
#define RH_RF95_REG_11_IRQ_FLAGS_MASK 0x11
#define RH_RF95_REG_12_IRQ_FLAGS 0x12
#define RH_RF95_REG_13_RX_NB_BYTES 0x13
#define RH_RF95_REG_1D_MODEM_CONFIG1 0x1D
#define RH_RF95_REG_1E_MODEM_CONFIG2 0x1E
#define RH_RF95_REG_1F_SYMB_TIMEOUT_LSB 0x1F
#define RH_RF95_REG_20_PREAMBLE_MSB 0x20
#define RH_RF95_REG_21_PREAMBLE_LSB 0x21
#define RH_RF95_REG_22_PAYLOAD_LENGTH 0x22
#define RH_RF95_REG_26_MODEM_CONFIG3 0x26
#define RH_RF95_REG_40_DIO_MAPPING1 0x40
#define RH_RF95_REG_42_VERSION 0x42
#define RH_RF95_REG_4D_PA_DAC 0x4D
#define RH_RF95_REG_19_PKT_SNR_VALUE 0x19
#define RH_RF95_REG_1A_PKT_RSSI_VALUE 0x1A
#define RH_RF95_DETECTION_OPTIMIZE 0x31
#define RH_RF95_DETECTION_THRESHOLD 0x37

// PA DAC values
#define RH_RF95_PA_DAC_DISABLE 0x04
#define RH_RF95_PA_DAC_ENABLE 0x07

// Frequency constants
#define RH_RF95_FXOSC 32000000.0
#define RH_RF95_FSTEP (RH_RF95_FXOSC / 524288.0)

// RadioHead constants
#define RH_BROADCAST_ADDRESS 0xFF
#define RH_FLAGS_ACK 0x80
#define RH_FLAGS_RETRY 0x40

// Operation modes
#define SLEEP_MODE 0b000
#define STANDBY_MODE 0b001
#define FS_TX_MODE 0b010
#define TX_MODE 0b011
#define FS_RX_MODE 0b100
#define RX_MODE 0b101

// Bandwidth bins
typedef enum {
    BW_7_8_KHZ = 0,
    BW_10_4_KHZ,
    BW_15_6_KHZ,
    BW_20_8_KHZ,
    BW_31_25_KHZ,
    BW_41_7_KHZ,
    BW_62_5_KHZ,
    BW_125_KHZ,
    BW_250_KHZ,
    BW_500_KHZ
} rfm9x_bandwidth_t;

// RFM9x configuration structure
typedef struct {
    spi_inst_t *spi;
    uint cs_pin;
    uint reset_pin;
    uint32_t frequency_hz;
    uint16_t preamble_length;
    bool high_power;
    uint32_t baudrate;
    bool auto_agc;
    bool enable_crc;

    // RadioHead compatibility
    uint8_t node;
    uint8_t destination;
    uint8_t identifier;
    uint8_t flags;
    uint8_t sequence_number;

    // Timeouts and delays
    float ack_wait;
    float receive_timeout;
    float xmit_timeout;
    uint8_t ack_retries;
    float ack_delay;

    // Statistics
    float last_rssi;
    float last_snr;
    uint16_t crc_error_count;

    // Internal state
    uint8_t seen_ids[256];
} rfm9x_t;

// Function declarations
bool rfm9x_init(rfm9x_t *rfm, spi_inst_t *spi, uint cs_pin, uint reset_pin, uint32_t frequency_hz);
void rfm9x_reset(rfm9x_t *rfm);
void rfm9x_sleep(rfm9x_t *rfm);
void rfm9x_idle(rfm9x_t *rfm);
void rfm9x_listen(rfm9x_t *rfm);
void rfm9x_transmit(rfm9x_t *rfm);

// Register access functions
uint8_t rfm9x_read_u8(rfm9x_t *rfm, uint8_t address);
void rfm9x_write_u8(rfm9x_t *rfm, uint8_t address, uint8_t value);
void rfm9x_read_into(rfm9x_t *rfm, uint8_t address, uint8_t *buffer, size_t length);
void rfm9x_write_from(rfm9x_t *rfm, uint8_t address, const uint8_t *buffer, size_t length);

// Property getters/setters
void rfm9x_set_frequency_mhz(rfm9x_t *rfm, float frequency_mhz);
float rfm9x_get_frequency_mhz(rfm9x_t *rfm);
void rfm9x_set_tx_power(rfm9x_t *rfm, int8_t power);
int8_t rfm9x_get_tx_power(rfm9x_t *rfm);
void rfm9x_set_signal_bandwidth(rfm9x_t *rfm, uint32_t bandwidth);
uint32_t rfm9x_get_signal_bandwidth(rfm9x_t *rfm);
void rfm9x_set_coding_rate(rfm9x_t *rfm, uint8_t rate);
uint8_t rfm9x_get_coding_rate(rfm9x_t *rfm);
void rfm9x_set_spreading_factor(rfm9x_t *rfm, uint8_t factor);
uint8_t rfm9x_get_spreading_factor(rfm9x_t *rfm);

// Status functions
bool rfm9x_tx_done(rfm9x_t *rfm);
bool rfm9x_rx_done(rfm9x_t *rfm);
bool rfm9x_crc_error(rfm9x_t *rfm);
int rfm9x_get_rssi(rfm9x_t *rfm);
float rfm9x_get_snr(rfm9x_t *rfm);

// Communication functions
bool rfm9x_send(rfm9x_t *rfm, const uint8_t *data, size_t length, bool keep_listening);
bool rfm9x_send_with_ack(rfm9x_t *rfm, const uint8_t *data, size_t length);
int rfm9x_receive(rfm9x_t *rfm, uint8_t *buffer, size_t buffer_size, bool keep_listening, bool with_header,
                  bool with_ack, uint32_t timeout_ms);

#endif  // RFM9X_H
