#ifndef BLOCKS__RFM95_BASE_H_
#define BLOCKS__RFM95_BASE_H_

/// RFM9x radio frequency (915 MHz)
#define RADIO_FREQUENCY 915000000

/// RFM9x radio configuration parameters
#define RADIO_SPREADING_FACTOR 8
#define RADIO_SIGNAL_BANDWIDTH 125000
#define RADIO_CODING_RATE 5

/// SPI configuration
#define RADIO_SPI_BAUDRATE (2000 * 2000)

#endif
