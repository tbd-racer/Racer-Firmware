#ifndef BOARDS__MK2__BLOCK__RP2040_CAN_BLOCK_H_
#define BOARDS__MK2__BLOCK__RP2040_CAN_BLOCK_H_

#include "blocks/rp2040_base.h"

#define STATUS_LEDR_PIN 13
#define STATUS_LEDG_PIN 14
#define STATUS_LEDB_PIN 15

#define FAULT_LED_PIN STATUS_LEDR_PIN
#define FAULT_LED_INVERTED 1

#define MCP2517FD_SPI 0
#define MCP2517FD_NCS_PIN 17
#define MCP2517FD_SCK_PIN 18
#define MCP2517FD_MOSI_PIN 19
#define MCP2517FD_MISO_PIN 20
#define MCP2517FD_CANCLK_PIN 21
#define MCP2517FD_INT_PIN 22

#define MCP2517FD_TERM_SENSE_ON_INT0 1

// CAN Bus Definitions
// Note that each can bus is defined by `BUS_NAME`_`PARAMETER_NAME`
// The following parameters are required: ENABLE_FD, RATE, and FD_RATE (if ENABLE_FD is 1)
// These names are referred to in the board definition files
#define INTERNAL_CAN_ENABLE_FD 1
#define INTERNAL_CAN_RATE 1000000
#define INTERNAL_CAN_FD_RATE 5000000
#define INTERNAL_CAN_ID 1

#define EXTERNAL_CAN_ENABLE_FD 0
#define EXTERNAL_CAN_RATE 250000
#define EXTERNAL_CAN_ID 2

#endif
