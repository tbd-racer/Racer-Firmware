#include "driver/led.h"
#include "pico/stdlib.h"
#include "titan/logger.h"
#include "titan/version.h"

#undef LOGGING_UNIT_NAME
#define LOGGING_UNIT_NAME "main"

#define EVENT_TIME 200

// Initialize all to nil time
// For background timers, they will fire immediately
// For ros timers, they will be reset before being ticked by start_ros_timers
absolute_time_t next_event = { 0 };
bool toggle = false;

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

static void tick_background_tasks() {
    if (timer_ready(&next_event, EVENT_TIME, false)) {
        toggle = !toggle;
        led_network_online_set(toggle);

        // do something
        led_update_pins();
    }
}

int main() {
    // Initialize stdio
    stdio_init_all();
    LOG_INFO("%s", FULL_BUILD_TAG);

    led_init();
    led_fault_set(false);
    led_network_enabled_set(true);
    led_network_online_set(true);

    // Enter main loop
    while (true) {
        // Do background tasks
        tick_background_tasks();
    }

    return 0;
}
