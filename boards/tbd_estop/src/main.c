#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <pico/stdio.h>
#include <pico/stdio_usb.h>
#include <pico/time.h>
#include <pico/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/rfm9x.h"
#include "driver/ws2812.h"
#include "pico/binary_info.h"
#include "titan/logger.h"
#include "titan/version.h"

#undef LOGGING_UNIT_NAME
#define LOGGING_UNIT_NAME "main"

bi_decl(bi_3pins_with_func(RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_SCK_PIN, GPIO_FUNC_SPI));
bi_decl(bi_1pin_with_name(RADIO_CS_PIN, "RADIO CS"));
bi_decl(bi_1pin_with_name(BUTTON_LED_PIN, "Kill LED"));
bi_decl(bi_1pin_with_name(BUTTON_STAT_PIN, "Kill BTN"));

/**
 * @brief Check if a timer is ready. If so advance it to the next interval.
 *
 * This will also raise a fault if timers are missed
 *
 * @param next_fire_ptr A pointer to the absolute_time_t holding the time the timer should next fire
 * @param interval_ms The interval the timer fires at
 * @return true The timer has fired, any action which was waiting for this timer should occur
 * @return false The timer has not fired
 */
static bool timer_ready(absolute_time_t *next_fire_ptr, uint32_t interval_ms, bool error_on_miss) {
    absolute_time_t time_tmp = *next_fire_ptr;
    if (time_reached(time_tmp)) {
        bool is_first_fire = is_nil_time(time_tmp);
        time_tmp = delayed_by_ms(time_tmp, interval_ms);
        if (time_reached(time_tmp)) {
            unsigned int i = 0;
            while (time_reached(time_tmp)) {
                time_tmp = delayed_by_ms(time_tmp, interval_ms);
                i++;
            }
            if (!is_first_fire) {
                LOG_WARN("Missed %u runs of %s timer 0x%p", i, (error_on_miss ? "critical" : "non-critical"),
                         next_fire_ptr);
            }
        }
        *next_fire_ptr = time_tmp;
        return true;
    } else {
        return false;
    }
}

// timers
absolute_time_t next_led_tick;
absolute_time_t next_btn_tick;
absolute_time_t next_radio_xmit;
absolute_time_t next_kill_state_change;

rfm9x_t radio;
union ws2812_command commands[BUTTON_NUM_LEDS];

bool kill_button_irq_trigger = false;
void gpio_irq(uint gpio, uint32_t events) {
    if (gpio == BUTTON_STAT_PIN && get_absolute_time() > next_kill_state_change) {
        // Indicate the IRQ fired, and start a debounce lockout
        kill_button_irq_trigger = true;
        next_kill_state_change = make_timeout_time_ms(300);
    }
}

enum tick_type_t { TICK_TYPE_A, TICK_TYPE_B } tick_type;
void update_btn_led(bool required, bool kill_asserting) {
    if (required) {
        if (kill_asserting) {
            // If required and killed show solid red
            commands[0].data = 0u;
            commands[0].cmd.red = 50u;
        } else {
            // if not asserting show solid green
            commands[0].data = 0u;
            commands[0].cmd.green = 50u;
        }
    } else {
        if (tick_type == TICK_TYPE_A) {
            // A tick is always yellow
            commands[0].data = 0u;
            commands[0].cmd.red = 50u;
            commands[0].cmd.green = 50u;
        } else if (kill_asserting && tick_type == TICK_TYPE_B) {
            // If not required, but killed show red on B tick
            commands[0].data = 0u;
            commands[0].cmd.red = 50u;

        } else {
            // if not required, and active show green on B tick
            commands[0].data = 0u;
            commands[0].cmd.green = 50u;
        }
    }

    ws2812_strip_set(commands);

    // Handling multi color modes
    if (tick_type == TICK_TYPE_A) {
        tick_type = TICK_TYPE_B;
    } else {
        tick_type = TICK_TYPE_A;
    }
}

int main() {
    // Setup the button LED
    ws2812_init(pio0, 0, BUTTON_LED_PIN, BUTTON_NUM_LEDS);

    // setup button pin
    gpio_init(BUTTON_STAT_PIN);
    gpio_set_dir(BUTTON_STAT_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_STAT_PIN);

    // config the GPIO IRQ to help with the button
    gpio_set_irq_enabled_with_callback(BUTTON_STAT_PIN, GPIO_IRQ_EDGE_RISE, true, &gpio_irq);

    // Initialize stdio
    stdio_init_all();

    // wait for either usb serial, a button press or 10s
    while (to_ms_since_boot(get_absolute_time()) < 10000) {
        LOG_INFO("Waiting for serial...");

        commands[0].data = 0u;
        commands[0].cmd.red = 50u;
        commands[0].cmd.green = 50u;
        ws2812_strip_set(commands);

        sleep_ms(250);

        commands[0].data = 0u;
        ws2812_strip_set(commands);

        sleep_ms(250);

        // handle exit conditions
        if (stdio_usb_connected() || kill_button_irq_trigger) {
            kill_button_irq_trigger = false;
            break;
        }
    }

    LOG_INFO("%s", FULL_BUILD_TAG);
    LOG_INFO("Initializing radio");

    // setup hardware spi 0
    gpio_set_function(RADIO_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RADIO_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RADIO_SCK_PIN, GPIO_FUNC_SPI);
    spi_init(spi0, RADIO_SPI_BAUDRATE);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Initialize RFM95 radio
    if (!rfm9x_init(&radio, spi0, RADIO_CS_PIN, RADIO_RST_PIN, RADIO_FREQUENCY)) {
        panic("failed radio init");
    }

    // Configure radio parameters
    rfm9x_set_spreading_factor(&radio, RADIO_SPREADING_FACTOR);
    rfm9x_set_signal_bandwidth(&radio, RADIO_SIGNAL_BANDWIDTH);
    rfm9x_set_coding_rate(&radio, RADIO_CODING_RATE);

    LOG_INFO("Radio init complete");

    bool kill_state_asserting = true;
    bool require_kill = true;

    uint16_t hold_count = 0;

    // show the correct led state
    update_btn_led(require_kill, kill_state_asserting);

    while (true) {
        if (timer_ready(&next_btn_tick, 100, false)) {
            // handle a counter for changing kill requirement
            if (gpio_get(BUTTON_STAT_PIN)) {
                hold_count++;
            } else if (hold_count > 0) {
                hold_count--;
            }

            // if we hit the threshold toggle the requirement (5s)
            if (hold_count > 50) {
                hold_count = 0;
                require_kill = !require_kill;
                LOG_INFO("Toggling kill requirement: %s", (require_kill ? "required" : "not required"));
            }
        }

        // Toggling via the button triggers the irq
        if (kill_button_irq_trigger) {
            kill_button_irq_trigger = false;

            // toggle the kill state
            kill_state_asserting = !kill_state_asserting;

            if (kill_state_asserting) {
                LOG_INFO("Asserting kill");
            } else {
                LOG_INFO("Clearing kill");
            }

            // Also force an LED update
            update_btn_led(require_kill, kill_state_asserting);
        }

        if (timer_ready(&next_led_tick, 250, false)) {
            update_btn_led(require_kill, kill_state_asserting);
        }

        // Send a radio packet when ready
        if (timer_ready(&next_radio_xmit, 100, true)) {
            uint8_t message[3] = { RADIO_MAGIC_BYTE, REM_KILLSWITCH_ID, 0x00 };
            // status byte = 000(required)000(is_asserting)
            message[2] = ((uint8_t)require_kill << 4) | (uint8_t)kill_state_asserting;

            // Send the message
            rfm9x_send(&radio, message, 3, false);
        }

        // establish a sleep to conserve power
        // sleep_ms(1);
    }

    return 0;
}
