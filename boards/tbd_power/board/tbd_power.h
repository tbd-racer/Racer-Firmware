#ifndef BOARDS__TBD_BMS_H_
#define BOARDS__TBD_BMS_H_

#include "blocks/rp2040_can_block.h"

#define PICO_TARGET_NAME "tbd_power"

// This defines which CAN bus this board is connected into
// The CAN bus is defined in the corresponding robot definition files (rate, enable FD, etc.)
#define CAN_BUS_NAME EXTERNAL_CAN
#define CAN_BUS_CLIENT_ID 1
#define MCP2517FD_TERM_SENSE_ON_INT0 0 // intentionally disable term sense

// Define custom client lookup for the bootloader (since we need to detect which board we're on)
#define TITAN_BOOTLOADER_CUSTOM_CLIENT_LOOKUP "can_bl_custom_id/sbh_mcu.h"

// Standard GPIO
#define PACK1_ACTIVE_PIN 0
#define PACK2_ACTIVE_PIN 1
#define AGX_PWR_CTL_PIN  2
#define LIDR_PWR_CTL_PIN 3
#define NET_PWR_CTL_PIN  4
#define NANO_PWR_CTL_PIN 5

// Radio pins
#define RADIO_NCS_PIN   7
#define RADIO_MISO_PIN  8
#define RADIO_RST_PIN   9
#define RADIO_SCK_PIN  10
#define RADIO_MOSI_PIN 11
#define RADIO_SPI_INST  1

// Power ADC connection
#define PERIPH_I2C          0
#define PERIPH_SDA_PIN     24
#define PERIPH_SCL_PIN     25

#ifndef PICO_DEFAULT_I2C
#define PICO_DEFAULT_I2C PERIPH_I2C
#endif
#ifndef PICO_DEFAULT_I2C_SDA_PIN
#define PICO_DEFAULT_I2C_SDA_PIN PERIPH_SDA_PIN
#endif
#ifndef PICO_DEFAULT_I2C_SCL_PIN
#define PICO_DEFAULT_I2C_SCL_PIN PERIPH_SCL_PIN
#endif

#endif