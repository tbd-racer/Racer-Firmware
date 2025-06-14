#ifndef BOARDS__TBD_BMS_H_
#define BOARDS__TBD_BMS_H_

#include "blocks/rp2040_can_block.h"

#define PICO_TARGET_NAME "tbd_power"

// This defines which CAN bus this board is connected into
// The CAN bus is defined in the corresponding robot definition files (rate, enable FD, etc.)
#define CAN_BUS_NAME EXTERNAL_CAN
#define CAN_BUS_CLIENT_ID 1

// Define custom client lookup for the bootloader (since we need to detect which board we're on)
#define TITAN_BOOTLOADER_CUSTOM_CLIENT_LOOKUP "can_bl_custom_id/sbh_mcu.h"

#define PERIPH_I2C          0
#define PERIPH_SDA_PIN      0
#define PERIPH_SCL_PIN      1

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