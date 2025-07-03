#ifndef BOARDS__TBD_BMS_H_
#define BOARDS__TBD_BMS_H_

#define PICO_TARGET_NAME "tbd_estop"

// Radio configs
#include "blocks/rfm95_base.h"

// Button PINS
#define BUTTON_STAT_PIN 25
#define BUTTON_LED_PIN 24 // (CJT) IDK

// Radio connections
#define RADIO_MISO_PIN 16
#define RADIO_MOSI_PIN 19
#define RADIO_SCK_PIN 18
#define RADIO_CS_PIN 17
#define RADIO_IRQ_PIN 15
#define RADIO_RST_PIN 14



#endif