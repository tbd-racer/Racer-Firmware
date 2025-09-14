#ifndef ADS7828_H
#define ADS7828_H

#include "stdint.h"

#define ADS7828_I2C_PORT PERIPH_I2C
#define ADS7828_I2C_ADDR 0b10010000
#define ADS7828_TIMEOUT_MS 20

#define ADS7828_WRITE_ERROR 0xffff
#define ADS7828_READ_ERROR 0xfffe

uint8_t ads7828_init();

/// Read ADC channel in blocking fashion
/// @param channel Channel to read (0-7)
/// @return ADC reading (12-bit value) or ADS7828_READING_ERROR on failure
uint16_t ads7828_read_channel_blocking(uint8_t channel);

#endif
