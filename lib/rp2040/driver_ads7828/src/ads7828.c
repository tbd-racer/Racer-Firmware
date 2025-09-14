#include "driver/ads7828.h"
#include <assert.h>

#include "hardware/i2c.h"
#include "pico/platform.h"

#define ADS7828_READ_LEN 2
#define ADS7828_WRITE_LEN 1
#define ADS7828_I2C_INST __CONCAT(i2c, ADS7828_I2C_PORT)

static const uint8_t ch_read_bytes[] = {
    0b0000, 0b0100, 0b0001, 0b0101, 0b0010, 0b0110, 0b0011, 0b0111,
};

uint8_t ads7828_init() {
    /// Perform initial read to start the ADC conversion
    ads7828_read_channel_blocking(0);
    return 0;
}

uint8_t make_channel_command(uint8_t channel) {
    /// Sets up a single ended read command. Keeps A/D conv & internal ref on
    return 0b10001100 | (ch_read_bytes[channel] << 4);
}

uint16_t ads7828_read_channel_blocking(uint8_t channel) {
    uint8_t tx_buf[ADS7828_WRITE_LEN];
    uint8_t rx_buf[ADS7828_READ_LEN];
    
    /// Check channel bounds
    assert(channel < 8);
    
    /// Prepare command byte for the channel
    tx_buf[0] = make_channel_command(channel);
    
    /// Write command to select channel (also triggers conversion)
    int ret = i2c_write_blocking(ADS7828_I2C_INST, ADS7828_I2C_ADDR >> 1, tx_buf, ADS7828_WRITE_LEN, false);
    if (ret == PICO_ERROR_GENERIC) {
        return ADS7828_WRITE_ERROR;
    }
    
    /// Read the previous conversion result
    ret = i2c_read_blocking(ADS7828_I2C_INST, ADS7828_I2C_ADDR >> 1, rx_buf, ADS7828_READ_LEN, false);
    if (ret == PICO_ERROR_GENERIC) {
        return ADS7828_READ_ERROR;
    }
    
    /// Combine bytes and return 12-bit result
    return (rx_buf[0] << 8) | rx_buf[1];
}