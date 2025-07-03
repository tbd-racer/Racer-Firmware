#ifndef BOARDS__TBD_BMS_H_
#define BOARDS__TBD_BMS_H_

#define PICO_TARGET_NAME "tbd_estop"

#include "blocks/rfm95_base.h"

// status LEDs 
#define PICO_DEFAULT_LED_PIN 25

// Radio connections
#define RADIO_MISO_PIN 16
#define RADIO_MOSI_PIN 19
#define RADIO_SCK_PIN 18
#define RADIO_CS_PIN 17
#define RADIO_IRQ_PIN 15
#define RADIO_RST_PIN 14
#define RADIO_SPI_INST 0

#endif