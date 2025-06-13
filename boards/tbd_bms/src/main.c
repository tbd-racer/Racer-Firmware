#include "pico/stdlib.h"

#include "driver/async_i2c.h"
#include "driver/canbus.h"
#include "driver/led.h"
#include "driver/bq40z80.h"
#include "micro_ros_pico/transport_can.h"
#include "titan/logger.h"
#include "titan/version.h"

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
#define PWRCYCL_CHECK_INTERVAL_MS 250
#define PWR_CYCLE_DURATION_MS 10000
#define DISPLAY_UPDATE_INTERVAL_MS 1000

// Initialize all to nil time
// For background timers, they will fire immediately
// For ros timers, they will be reset before being ticked by start_ros_timers
absolute_time_t next_heartbeat = {0};
absolute_time_t next_status_update = {0};
absolute_time_t next_led_update = {0};
absolute_time_t next_connect_ping = {0};
absolute_time_t next_pack_present_update = {0};
absolute_time_t next_battery_status_update = {0};
absolute_time_t next_pwrcycl_update = {0};
absolute_time_t next_display_update = {0};

uint8_t presence_fail_count = 0;
bq_pack_info_t bq_pack_info;
uint can_id;

static void start_ros_timers(){
    
}

/**
 * @brief Ticks all ROS related code
 */
static void tick_ros_tasks() {

}

static void tick_background_tasks() {
    canbus_tick();


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
    LOG_INFO("%s", FULL_BUILD_TAG);

    // Perform all initializations
    // NOTE: Safety must be the first thing up after stdio, so the watchdog will be enabled
    safety_setup();
    led_init();
    micro_ros_init_error_handling();
    async_i2c_init(PERIPH_SDA_PIN, PERIPH_SCL_PIN, -1, -1, 400000, 20);

    sleep_ms(1000);
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