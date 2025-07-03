#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "hardware/i2c.h"

#include "titan/logger.h"

#include "driver/bq40z80.h"

#undef LOGGING_UNIT_NAME
#define LOGGING_UNIT_NAME "bq40z80"

#define RETRANSMIT_COUNT 4

#define BQ40Z80_I2C_INST __CONCAT(i2c, BQ40Z80_I2C_PORT)

static uint8_t bq_handle_i2c_transfer(uint8_t* bq_reg, uint8_t* rx_buf, uint len){
    int ret_code = PICO_ERROR_GENERIC;
    uint retries = 0;

    // slow down the transfers, smbus and pio no likey :/
    // sleep_ms(1);

    while(ret_code == PICO_ERROR_GENERIC && retries < RETRANSMIT_COUNT){
        // send the request to the chip
        ret_code = i2c_write_blocking(BQ40Z80_I2C_INST, BQ_ADDR, bq_reg, 1, false);
        if(ret_code == PICO_ERROR_GENERIC){
            sleep_ms(3);
            retries ++;
            continue;
        }

        ret_code = i2c_read_blocking(BQ40Z80_I2C_INST, BQ_ADDR, rx_buf, len, false);
        if(ret_code == PICO_ERROR_GENERIC){
            // let i2c relax a sec, something with smbus and the chip being busy
            sleep_ms(3);
            retries ++;
        }
    }

    if(ret_code == PICO_ERROR_GENERIC){
        //something bad happened to the i2c, panic
        LOG_FATAL("I2C transfer error: %d", ret_code);
        panic("I2C NACK during transfer %d times", RETRANSMIT_COUNT);
    }

    return retries;
}

uint8_t bq_write_only_transfer(uint8_t* tx_buf, uint len){
    int ret_code = -1; // if you set this to zero, the compiler *will delete* this function
    uint retries = 0;

    // slow down the transfers, smbus and pio no likey :/
    sleep_ms(1);

    while(ret_code && retries < RETRANSMIT_COUNT){
        // send the request to the chip
        ret_code = i2c_write_blocking(BQ40Z80_I2C_INST, BQ_ADDR, tx_buf, len, false);
        if(ret_code == PICO_ERROR_GENERIC){
            // let i2c relax a sec, something with smbus and the chip being busy
            sleep_ms(3);
            retries ++;
        }
    }

    if(ret_code){
        //something bad happened to the i2c, panic
        LOG_FATAL("I2C write error: %d", ret_code);
        panic("I2C NACK during MAC_WRITE %d times", RETRANSMIT_COUNT);
    }

    return retries;
}

uint8_t bq_init() {
    uint8_t retries = 0;

    // Init the wake pin, active low
    gpio_init(BMS_WAKE_PIN);
    gpio_set_dir(BMS_WAKE_PIN, GPIO_OUT);
    gpio_put(BMS_WAKE_PIN, 0);

    // init I2C
    i2c_init(BQ40Z80_I2C_INST, 100 * 1000);
    gpio_set_function(BMS_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(BMS_SCL_PIN, GPIO_FUNC_I2C);

    // make the request for the serial #
    uint8_t data[2] = {BQ_READ_CELL_SERI, 0x00};

    // Start the I2C to the chip
    while((data[1] == 0x00 || data[1] == 0xFF) && retries < 3) {
        int ret_code = 0;

        // send the request to the chip
        ret_code = i2c_write_blocking(BQ40Z80_I2C_INST, BQ_ADDR, data, 1, false);
        if(ret_code == PICO_ERROR_GENERIC){
            gpio_put(BMS_WAKE_PIN, 1);
            sleep_ms(1000);
            gpio_put(BMS_WAKE_PIN, 0);
            retries++;
            continue;
        }

        // Check for valid data
        ret_code = i2c_read_blocking(BQ40Z80_I2C_INST, BQ_ADDR, data, 2, false);
        if(ret_code == PICO_ERROR_GENERIC || data[0] == 0x00 || data[0] == 0xFF) {
            gpio_put(BMS_WAKE_PIN, 1);
            sleep_ms(1000);
            gpio_put(BMS_WAKE_PIN, 0);
            retries++;
        } else {
            // if we get here, we found a valid bq40z80
            break;
        }

    }
    if(retries == 3)
        return retries;

    return 0;
}

uint8_t bq_pack_present(){
    // read the operationstatus register (1-byte length + 4-byte data)
    uint8_t data[5] = {0, 0, 0, 0, 0};
    uint8_t reg_addr[1] = {BQ_READ_OPER_STAT};
    bq_handle_i2c_transfer(reg_addr, data, 5);

    // the zeroth byte is the length of the field for some reason...
    // test the presence bit (bit 0 of byte 1)
    return (uint8_t)(data[1] & 0b00000001);
}

uint8_t bq_pack_discharging(){
    // read the operationstatus register (32 bits)
    uint8_t data[5] = {0, 0, 0, 0, 0};
    uint8_t reg_addr[1] = {BQ_READ_OPER_STAT};
    bq_handle_i2c_transfer(reg_addr, data, 5);

    // the zeroth byte is the length of the field for some reason...
    // test the discharge bit (bit 1)
    return (uint8_t)(data[1] & 0b00000010);
}

uint8_t bq_pack_soc() {
    // read the SOC from the pack
    uint8_t data[1] = {0};
    uint8_t reg_addr[1] = {BQ_READ_RELAT_SOC};
    bq_handle_i2c_transfer(reg_addr, data, 1);

    return data[0];
}

uint16_t bq_pack_voltage(){
    // read the SOC from the pack
    uint8_t data[2] = {0, 0};
    uint8_t reg_addr[1] = {BQ_READ_PACK_VOLT};
    bq_handle_i2c_transfer(reg_addr, data, 2);


    uint16_t millivolts = data[0] | data[1] << 8;
    return millivolts;
}

int16_t bq_pack_current(){
    // read the SOC from the pack
    uint8_t data[2] = {0, 0};
    uint8_t reg_addr[1] = {BQ_READ_PACK_CURR};
    bq_handle_i2c_transfer(reg_addr, data, 2);

    int16_t current_ma = data[0] | data[1] << 8;
    return current_ma * 4; //mutliply by 4 to true reading
}

int16_t bq_avg_current(){
    // read the SOC from the pack
    uint8_t data[2] = {0, 0};
    uint8_t reg_addr[1] = {BQ_READ_AVRG_CURR};
    bq_handle_i2c_transfer(reg_addr, data, 2);

    int16_t current_ma = data[0] | data[1] << 8;
    return current_ma * 4; // have to * 4 to get true value
}

uint16_t bq_time_to_empty(){
    // read the SOC from the pack
    uint8_t data[2] = {0, 0};
    uint8_t reg_addr[1] = {BQ_READ_TIME_EMPT};
    bq_handle_i2c_transfer(reg_addr, data, 2);

    uint16_t runtime_mins = data[0] | data[1] << 8;
    return runtime_mins;
}

struct bq_pack_info_t bq_pack_mfg_info(){
    struct bq_pack_info_t pack_info;

    // generic data buffer for all msg info transfers
    uint8_t data[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // read the mfg serial #
    uint8_t reg_addr[1] = {BQ_READ_CELL_DATE};
    bq_handle_i2c_transfer(reg_addr, data, 2);

    // capture the manufacture date and make it more usable
    uint16_t cell_date_raw = data[0] | data[1] << 8;
    pack_info.mfg_day = cell_date_raw & 0x1F;
    pack_info.mfg_mo = (cell_date_raw >> 5) & 0x0F;
    pack_info.mfg_year = (cell_date_raw >> 9) + 1980;

    // read the SOC from the pack
    reg_addr[0] = BQ_READ_CELL_SERI;
    bq_handle_i2c_transfer(reg_addr, data, 2);

    pack_info.serial = data[0] | data[1] << 8;

    // read the SOC from the pack
    reg_addr[0] = BQ_READ_CELL_NAME;
    bq_handle_i2c_transfer(reg_addr, data, 21);

    // move the info from the pack read back into the mfg data
    for(uint8_t i = 1; i < data[0]+1; i++){
        pack_info.name[i-1] = data[i];
    }
    pack_info.name[data[0]+1] = 0;

    // now kick it all out
    return pack_info;
}

void bq_send_mac_command(const uint16_t command){
    // pack the MAC command
    uint8_t data[3] = {
        0x00, // MAC register address (actually 1 byte not 2 so cut off the upper byte)
        (uint8_t)(command >> 8),           // the high byte of the command
        (uint8_t)(command & 0xFF),         // the low byte of the command
    };

    // send the MAC register command
    bq_write_only_transfer(data, 3);
}

// WARNING! This is a blocking call. It should block for around 10s. It will continue to feed the
// watchdog during execution as to not time the system out. This must be done to maintain MAC synchronicity
// with the BQ chip during manual fet control
void bq_open_dschg_temp(const int64_t open_time_ms){
    // test operation status to determine if output is not on
    if(! bq_pack_discharging()){
        // bail, dont want to tun on the output manually
        return;
    }

    // send Emergency FET override command to take manual control
    bq_send_mac_command(BQ_MAC_EMG_FET_CTRL_CMD);

    // send emergency fet off command
    bq_send_mac_command(BQ_MAC_EMG_FET_OFF_CMD);

    absolute_time_t fet_off_command_end = make_timeout_time_ms(open_time_ms);
    while(bq_pack_discharging() && !time_reached(fet_off_command_end)){
        sleep_us(100);
        // also feed the watchdog so we dont reset
        watchdog_update();
    }

    uint8_t data[2] = {0, 0};
    uint8_t reg_addr[1] = {0x16};
    bq_handle_i2c_transfer(reg_addr, data, 2);
    if(data[0] & 0x7) {
        char err[9] = "ERROR:#|#";
        itoa(data[0] & 0x7, err + 6, 10);
        *(err + 7) = '|';
        itoa(data[1] & 0x7, err + 8, 10);
        panic(err);
    }

    // verify fet is actually open via operation status
    if(bq_pack_discharging()){
        // this is a bad state. we requested fet open and it didnt happen
        bq_send_mac_command(BQ_MAC_RESET_CMD);
        panic("Failed to open DSCHG FET during power cycle cmd");
    }

    // send a cleanup reset command
    bq_send_mac_command(BQ_MAC_EMG_FET_ON_CMD);
}