#include "pico/stdlib.h"

#include "pico/stdio_usb.h"

#include "driver/async_i2c.h"
#include "driver/canbus.h"
#include "driver/led.h"
#include "driver/bq40z80.h"
#include "micro_ros_pico/transport_can.h"
#include "titan/logger.h"
#include "titan/version.h"

#include "display.h"
#include "ros.h"
#include "safety_interface.h"

#undef LOGGING_UNIT_NAME
#define LOGGING_UNIT_NAME "main"

#define UROS_CONNECT_PING_TIME_MS 1000
#define HEARTBEAT_TIME_MS 100
#define FIRMWARE_STATUS_TIME_MS 1000
#define BATTERY_STATUS_TIME_MS 1000
#define LED_UPTIME_INTERVAL_MS 250
#define PRESENCE_CHECK_INTERVAL_MS 1000
#define PRESENCE_TIMEOUT_COUNT 10
#define DISPLAY_UPDATE_INTERVAL_MS 1000

// Initialize all to nil time
// For background timers, they will fire immediately
// For ros timers, they will be reset before being ticked by start_ros_timers
absolute_time_t next_heartbeat = {0};
absolute_time_t next_status_update = {0};
absolute_time_t next_led_update = {0};
absolute_time_t next_connect_ping = {0};
absolute_time_t next_battery_status_update = {0};
absolute_time_t next_shutdown_update = {0};
absolute_time_t next_display_update = {0};

uint8_t presence_fail_count = 0;
bq_pack_info_t bq_pack_info;
uint can_id;

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
                if (error_on_miss)
                    safety_raise_fault(FAULT_TIMER_MISSED);
            }
        }
        *next_fire_ptr = time_tmp;
        return true;
    }
    else {
        return false;
    }
}

static void start_ros_timers(){
    next_heartbeat = make_timeout_time_ms(HEARTBEAT_TIME_MS);
    next_status_update = make_timeout_time_ms(FIRMWARE_STATUS_TIME_MS);
    next_battery_status_update = make_timeout_time_ms(BATTERY_STATUS_TIME_MS);
}

/**
 * @brief Ticks all ROS related code
 */
static void tick_ros_tasks() {
    if (timer_ready(&next_heartbeat, HEARTBEAT_TIME_MS, true)) {
        // RCSOFTRETVCHECK is used as important logs should occur within ros.c,
        RCSOFTRETVCHECK(ros_heartbeat_pulse(can_id));
    }

    // send the firmware status updates
    if (timer_ready(&next_status_update, FIRMWARE_STATUS_TIME_MS, true)) {
        RCSOFTRETVCHECK(ros_update_firmware_status(can_id));
    }

    // send the battery status updates
    if (timer_ready(&next_battery_status_update, BATTERY_STATUS_TIME_MS, true)) {
        RCSOFTRETVCHECK(ros_update_battery_status(bq_pack_info, can_id));
    }
}

static void tick_background_tasks() {
    canbus_tick();

    if (timer_ready(&next_led_update, LED_UPTIME_INTERVAL_MS, false)) {
        // update the RGB led
        led_network_online_set(canbus_check_online());
    }

    // Determine if we need a shutdown
    if (timer_ready(&next_shutdown_update, PRESENCE_CHECK_INTERVAL_MS, false)) {
        // check if we need to update the presence counter
        if(!(canbus_check_online() || bq_pack_present())){
            presence_fail_count ++;
        } else {
            presence_fail_count = 0;
        }

        // if the presence counter times out, shut down
        if(presence_fail_count > PRESENCE_TIMEOUT_COUNT){
            LOG_WARN("Pack not detected after %ds. Powering down!", PRESENCE_TIMEOUT_COUNT);
            gpio_put(PWR_CTRL_PIN, 0);
        }
    }

    // Update LCD if reed switch held and we are in time for a display update
    if (gpio_get(SWITCH_SIGNAL_PIN) && time_reached(next_display_update)) {
        next_display_update = make_timeout_time_ms(DISPLAY_UPDATE_INTERVAL_MS);

        // Show pack info
        // display_show_stats(bq_pack_info.serial, bq_pack_soc(), bq_pack_voltage() / 1000.0);
    }
}

int main() {
    // Latch RP2040 power to on
    gpio_init(PWR_CTRL_PIN);
    gpio_set_dir(PWR_CTRL_PIN, GPIO_OUT);
    gpio_put(PWR_CTRL_PIN, 1);

    gpio_init(SWITCH_SIGNAL_PIN);
    gpio_set_dir(PWR_CTRL_PIN, GPIO_IN);

    // Initialize stdio
    stdio_init_all();
    stdio_usb_init();
    LOG_INFO("%s", FULL_BUILD_TAG);

    // Perform all initializations
    // NOTE: Safety must be the first thing up after stdio, so the watchdog will be enabled
    safety_setup();
    led_init();
    micro_ros_init_error_handling();
    async_i2c_init(PERIPH_SDA_PIN, PERIPH_SCL_PIN, -1, -1, 400000, 20);
    // display_init();

    safety_tick();

    // start the bq40z80
    int err = bq_init();
    if(err > 0) {
        LOG_ERROR("Failed to initialize the bq40z80 after %d attempts", err);
        panic("BQ40Z80 Init failed!");
    }

    // grab the pack info from the bq40z80
    bq_pack_info = bq_pack_mfg_info();
    LOG_INFO("pack %s, mfg %d/%d/%d, SER# %d", bq_pack_info.name, bq_pack_info.mfg_mo,
            bq_pack_info.mfg_day, bq_pack_info.mfg_year, bq_pack_info.serial);

    can_id = 0; // TODO update this from flash
    if (!transport_can_init(can_id)) {
        // No point in continuing onwards from here, if we can't initialize CAN hardware might as well panic and retry
        panic("Failed to initialize CAN bus hardware!");
    }

    sleep_ms(1000);

    // Enter main loop
    // This is split into two sections of timers
    // Those running with ROS, and those in the background
    // Note that both types of timers will need to conform to the minimal delay time, as there is around
    //   20ms of time worst case before the watchdog fires (as the ROS timeout is 30ms)
    // Meaning, don't block, either poll it in the background task or send it to an interrupt
    bool ros_initialized = false;
    while(true) {
        // Do background tasks
        tick_background_tasks();

        // Handle ROS state logic
        if(is_ros_connected()) {
            if(!ros_initialized) {
                LOG_INFO("ROS connected");

                // Lower all ROS related faults as we've got a new ROS context
                safety_lower_fault(FAULT_ROS_ERROR);

                if(ros_init(0) == RCL_RET_OK) { // TODO supply board serial number here
                    ros_initialized = true;
                    led_ros_connected_set(true);
                    safety_init();
                    start_ros_timers();
                    // display_show_ros_connect();
                } else {
                    LOG_ERROR("ROS failed to initialize.");
                    ros_fini();
                }
            } else {
                ros_spin_executor();
                tick_ros_tasks();
            }
        } else if(ros_initialized){
            LOG_INFO("Lost connection to ROS");
            ros_fini();
            safety_deinit();
            led_ros_connected_set(false);
            // display_show_ros_disconnect();

            ros_initialized = false;
        } else {
            if (time_reached(next_connect_ping)) {
                ros_ping();
                next_connect_ping = make_timeout_time_ms(UROS_CONNECT_PING_TIME_MS);
            }
        }

        // Tick safety
        safety_tick();

    }

    return 0;
}