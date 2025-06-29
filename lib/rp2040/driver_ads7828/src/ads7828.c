#include "driver/ads7828.h"

#include "driver/async_i2c.h"

#define ADS7828_READ_LEN 2
#define ADS7828_WRITE_LEN 1

static bool msg_in_progress = false;

static const uint8_t ch_read_bytes[] = {
    0b0000,
    0b0100,
    0b0001,
    0b0101,
    0b0010,
    0b0110,
    0b0011,
    0b0111,
};

uint8_t rx_buf[ADS7828_READ_LEN];
uint8_t tx_buf[ADS7828_WRITE_LEN];
static struct async_i2c_request channel_read_req = {.i2c_num = ADS7828_I2C_PORT,
                                                    .address = ADS7828_I2C_ADDR,
                                                    .nostop = false,
                                                    .tx_buffer = tx_buf,
                                                    .rx_buffer = rx_buf,
                                                    .bytes_to_send = ADS7828_WRITE_LEN,
                                                    .bytes_to_receive = ADS7828_READ_LEN,
                                                    .completed_callback = NULL,
                                                    .failed_callback = NULL,
                                                    .next_req_on_success = NULL};

uint8_t ads7828_init()
{
    // Initialize the buffer to invalid reading state
    rx_buf[0] = ADS7828_READING_INVL >> 8;
    rx_buf[1] = ADS7828_READING_INVL & 0xFF;

    ads7828_read_channel(0);
}

uint8_t make_channel_command(uint8_t channel)
{
    // Sets up a single ended read command. Keeps A/D conv & internal ref on
    return 0b10001100 | (ch_read_bytes[channel] << 4);
}

void ads7828_read_channel(uint8_t channel)
{
    tx_buf[0] = make_channel_command(channel);

    // init the data buffer to invalid
    rx_buf[0] = ADS7828_READING_INVL >> 8;
    rx_buf[1] = ADS7828_READING_INVL & 0xFF;
    async_i2c_enqueue(&channel_read_req, &msg_in_progress);
}

uint16_t ads7828_get_reading()
{
    // If in progress report notready
    if (msg_in_progress)
    {
        return ADS7828_READING_NRDY;
    }

    // otherwise report the value
    return rx_buf[0] << 8 | rx_buf[1];
}
