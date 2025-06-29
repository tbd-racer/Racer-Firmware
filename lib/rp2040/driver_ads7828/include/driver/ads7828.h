#ifndef ADS7828_H
#define ADS7828_H

#include "pico/stdlib.h"
#include "stdint.h"

#define ADS7828_I2C_PORT PERIPH_I2C
#define ADS7828_I2C_ADDR 0b10010000
#define ADS7828_TIMEOUT_MS 20

#define ADS7828_READING_NRDY 0xffff
#define ADS7828_READING_INVL 0xefff

uint8_t ads7828_init();

void ads7828_read_channel(uint8_t channel);

uint16_t ads7828_get_reading();

#endif
