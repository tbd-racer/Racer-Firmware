#include <chassis_msgs/srv/restart_power_channel.h>
#include <hardware/gpio.h>
#include <pico/types.h>

#include "driver/ads7828.h"
#include "driver/canbus.h"
#include "driver/led.h"
#include "driver/rfm9x.h"
#include "hardware/i2c.h"
#include "micro_ros_pico/transport_can.h"
#include "pico/stdlib.h"
#include "ros.h"
#include "safety_interface.h"
#include "titan/logger.h"
#include "titan/version.h"

#undef LOGGING_UNIT_NAME
#define LOGGING_UNIT_NAME "main"

#define UROS_CONNECT_PING_TIME_MS 1000
#define HEARTBEAT_TIME_MS 100
#define FIRMWARE_STATUS_TIME_MS 1000
#define KILLSWITCH_TIME_MS 100
#define ELECTRICAL_READINGS_TIME_MS 500
#define LED_UPTIME_INTERVAL_MS 250
#define CHANNEL_RESTART_TIME_MS 1000
#define RADIO_RX_TIMEOUT_MS 10

/// ADC Channel definitions for voltage monitoring
#define ADC_CHANNEL_BATTERY_1 4
#define ADC_CHANNEL_BATTERY_2 5
#define ADC_CHANNEL_INPUT_VOLTAGE 3
#define ADC_CHANNEL_REGULATOR_15V 0
#define ADC_CHANNEL_REGULATOR_12V 1
#define ADC_CHANNEL_REGULATOR_5V 2

/// Voltage divider scaling factors (these may need adjustment based on actual circuit)
#define VOLTAGE_SCALE_BATTERY 10.989f  // Scale factor for battery voltage readings
#define VOLTAGE_SCALE_5V 5.973f       // Scale factor for 5V regulator voltage readings
#define VOLTAGE_SCALE_12V 5.987f       // Scale factor for 12V regulator voltage readings
#define VOLTAGE_SCALE_15V 6.003f         // Scale factor for 15V regulator voltage readings
#define ADC_REF 2.5f / 4096.0f         // ADC reference voltage
#define MIN_INPUT_VOLTAGE 16.0f       // Minimum input voltage before shutdown

// rfm radio for estop communication
rfm9x_t radio;

// Flag to indicate if a channel restart is currently active
bool power_channel_restart_active = false;

// Initialize all to nil time
// For background timers, they will fire immediately
// For ros timers, they will be reset before being ticked by start_ros_timers
absolute_time_t next_heartbeat = { 0 };
absolute_time_t next_status_update = { 0 };
absolute_time_t next_kill_update = { 0 };
absolute_time_t next_electrical_update = { 0 };
absolute_time_t next_shutdown_check = { 0 };
absolute_time_t next_led_update = { 0 };
absolute_time_t next_connect_ping = { 0 };
absolute_time_t next_display_update = { 0 };
absolute_time_t next_channel_restart = { 0 };

/**
 * @brief Check if a timer is ready. If so advance it to the next interval.
 *
 * This will also raise a fault if timers are missed
 *
 * @param next_fire_ptr A pointer to the absolute_time_t holding the time the
 * timer should next fire
 * @param interval_ms The interval the timer fires at
 * @return true The timer has fired, any action which was waiting for this timer
 * should occur
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
                if (error_on_miss) {
                    safety_raise_fault(FAULT_TIMER_MISSED);
                }
            }
        }
        *next_fire_ptr = time_tmp;
        return true;
    } else {
        return false;
    }
}

static void start_ros_timers() {
    next_heartbeat = make_timeout_time_ms(HEARTBEAT_TIME_MS);
    next_status_update = make_timeout_time_ms(FIRMWARE_STATUS_TIME_MS);
    next_kill_update = make_timeout_time_ms(KILLSWITCH_TIME_MS);
    next_electrical_update = make_timeout_time_ms(ELECTRICAL_READINGS_TIME_MS);
}

/// @brief Convert ADC reading to voltage with scaling factor
/// @param adc_reading Raw ADC reading (0-4095)
/// @param scale_factor Voltage divider scaling factor
/// @return Voltage in volts
static float adc_to_voltage(uint16_t adc_reading, float scale_factor) {
    if (adc_reading == ADS7828_READ_ERROR) {
        LOG_WARN("ADC reading invalid");
        return 0.0f;  // Return 0 for not ready readings
    }
    if (adc_reading == ADS7828_WRITE_ERROR) {
        LOG_WARN("ADC reading not ready");
        return 0.0f;  // Return 0 for invalid readings
    }
    return ((float)adc_reading) * ADC_REF * scale_factor;
}

int mapROSPinToGPIOPin(int pin) {
    // Map ROS pin numbers to GPIO pin numbers
    switch (pin) {
        case chassis_msgs__srv__RestartPowerChannel_Request__LIDAR_CHANNEL:
            return LIDR_PWR_CTL_PIN;
        case chassis_msgs__srv__RestartPowerChannel_Request__JETSON_CHANNEL:
            return AGX_PWR_CTL_PIN;
        case chassis_msgs__srv__RestartPowerChannel_Request__AUX_CHANNEL:
            return NANO_PWR_CTL_PIN;
        case chassis_msgs__srv__RestartPowerChannel_Request__NETWORK_CHANNEL:
            return NET_PWR_CTL_PIN;
        default:
            return 255;  // Invalid pin, return an invalid GPIO pin
    }
}

/**
 * @brief Ticks all ROS related code
 */
static void tick_ros_tasks() {
    if (timer_ready(&next_heartbeat, HEARTBEAT_TIME_MS, true)) {
        // RCSOFTRETVCHECK is used as important logs should occur within ros.c,
        RCSOFTRETVCHECK(ros_heartbeat_pulse(CAN_BUS_CLIENT_ID));
    }

    // send the firmware status updates
    if (timer_ready(&next_status_update, FIRMWARE_STATUS_TIME_MS, true)) {
        RCSOFTRETVCHECK(ros_update_firmware_status(CAN_BUS_CLIENT_ID));
    }

    // Send killswitch updates
    if (timer_ready(&next_kill_update, KILLSWITCH_TIME_MS, true)) {
        RCSOFTRETVCHECK(ros_update_killswitches());
    }

    // Send electrical readings updates
    if (timer_ready(&next_electrical_update, ELECTRICAL_READINGS_TIME_MS, true)) {
        // Read all ADC channels and convert to voltages
        float battery_1_voltage = 0.0f;
        float battery_2_voltage = 0.0f;
        float input_voltage = 0.0f;
        float regulator_15v = 0.0f;
        float regulator_12v = 0.0f;
        float regulator_5v = 0.0f;

        // Read battery 1 voltage
        uint16_t adc_reading = ads7828_read_channel_blocking(ADC_CHANNEL_BATTERY_1);
        battery_1_voltage = adc_to_voltage(adc_reading, VOLTAGE_SCALE_BATTERY);

        // Read battery 2 voltage
        adc_reading = ads7828_read_channel_blocking(ADC_CHANNEL_BATTERY_2);
        battery_2_voltage = adc_to_voltage(adc_reading, VOLTAGE_SCALE_BATTERY);

        // Read input voltage
        adc_reading = ads7828_read_channel_blocking(ADC_CHANNEL_INPUT_VOLTAGE);
        input_voltage = adc_to_voltage(adc_reading, VOLTAGE_SCALE_BATTERY);

        // Read 15V regulator voltage
        adc_reading = ads7828_read_channel_blocking(ADC_CHANNEL_REGULATOR_15V);
        regulator_15v = adc_to_voltage(adc_reading, VOLTAGE_SCALE_15V);

        // Read 12V regulator voltage
        adc_reading = ads7828_read_channel_blocking(ADC_CHANNEL_REGULATOR_12V);
        regulator_12v = adc_to_voltage(adc_reading, VOLTAGE_SCALE_12V);

        // Read 5V regulator voltage
        adc_reading = ads7828_read_channel_blocking(ADC_CHANNEL_REGULATOR_5V);
        regulator_5v = adc_to_voltage(adc_reading, VOLTAGE_SCALE_5V);

        // Determine which battery is active based on GPIO pins
        bool is_battery_1 = !gpio_get(PACK1_ACTIVE_PIN);  // Assuming active low
        bool is_battery_2 = !gpio_get(PACK2_ACTIVE_PIN);  // Assuming active low

        // Publish electrical readings
        RCSOFTRETVCHECK(ros_update_electrical_readings(battery_1_voltage, battery_2_voltage, input_voltage,
                                                       regulator_15v, regulator_12v, regulator_5v, is_battery_1,
                                                       is_battery_2));
    }
}

static void tick_background_tasks() {
    canbus_tick();
    if (timer_ready(&next_led_update, LED_UPTIME_INTERVAL_MS, false)) {
        // update the RGB led
        led_network_online_set(canbus_check_online());
    }

    // Wait for CHANNEL_RESTART_TIME_MS to allow capacitors to discharge
    if (timer_ready(&next_channel_restart, CHANNEL_RESTART_TIME_MS, false)) {
        // Set the channel GPIO high
        gpio_put(mapROSPinToGPIOPin(channel_restart), 1);
        // Reset the channel restart request
        channel_restart = 255;
    }

    if(timer_ready(&next_shutdown_check, ELECTRICAL_READINGS_TIME_MS, true)) {
        // Check if the shutdown condition is met
        uint16_t adc_reading = ads7828_read_channel_blocking(ADC_CHANNEL_INPUT_VOLTAGE);
        float input_voltage = adc_to_voltage(adc_reading, VOLTAGE_SCALE_BATTERY);

        if(input_voltage > 0.5 && input_voltage < MIN_INPUT_VOLTAGE) {
            LOG_ERROR("Input voltage too low: %.2f V. Initiating shutdown.", input_voltage);
            safety_raise_fault(FAULT_UNDERVOLTAGE);

            gpio_put(AGX_PWR_CTL_PIN, 0);
            gpio_put(LIDR_PWR_CTL_PIN, 0);
            gpio_put(NET_PWR_CTL_PIN, 0);
            gpio_put(NANO_PWR_CTL_PIN, 0);
        } else {
            LOG_INFO("Input voltage OK: %.2f V.", input_voltage);
            safety_lower_fault(FAULT_UNDERVOLTAGE);
        }
    }
}

static void handle_radio_packets(uint8_t received, uint8_t packet_buffer[]) {
    // Handle radio kill packets
    if (packet_buffer[0] == RADIO_KILL_HDR && received == 3) {
        uint8_t id = packet_buffer[1];
        bool is_required = packet_buffer[2] & 0x10;
        bool is_asserting = packet_buffer[2] & 0x01;

        // Ensure ID is valid
        if (id > 0 && id < NUM_KILL_SWITCHES) {
            safety_kill_switch_update(id, is_asserting, is_required);
        } else {
            LOG_WARN("Unknown switch id: %x, req: %u, asrt: %u", id, is_required, is_asserting);
        }
    }
}

int main() {
    // Initialize stdio
    stdio_init_all();

    LOG_INFO("%s", FULL_BUILD_TAG);
    LOG_INFO("Initializing power board");

    i2c_init(__CONCAT(i2c, PERIPH_I2C), 400000);
    gpio_set_function(PERIPH_SCL_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PERIPH_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PERIPH_SCL_PIN);
    gpio_pull_up(PERIPH_SDA_PIN);

    LOG_INFO("NOU");

    // Perform all initializations
    // NOTE: Safety must be the first thing up after stdio, so the watchdog will
    // be enabled
    safety_setup();
    led_init();
    micro_ros_init_error_handling();
    ads7828_init();

    // Now pull up the GPIO
    gpio_init(PACK1_ACTIVE_PIN);
    gpio_set_dir(PACK1_ACTIVE_PIN, GPIO_IN);

    gpio_init(PACK2_ACTIVE_PIN);
    gpio_set_dir(PACK2_ACTIVE_PIN, GPIO_IN);

    gpio_init(AGX_PWR_CTL_PIN);
    gpio_set_dir(AGX_PWR_CTL_PIN, GPIO_OUT);
    gpio_put(AGX_PWR_CTL_PIN, 1);

    gpio_init(LIDR_PWR_CTL_PIN);
    gpio_set_dir(LIDR_PWR_CTL_PIN, GPIO_OUT);
    gpio_put(LIDR_PWR_CTL_PIN, 1);

    gpio_init(NET_PWR_CTL_PIN);
    gpio_set_dir(NET_PWR_CTL_PIN, GPIO_OUT);
    gpio_put(NET_PWR_CTL_PIN, 1);

    gpio_init(NANO_PWR_CTL_PIN);
    gpio_set_dir(NANO_PWR_CTL_PIN, GPIO_OUT);
    gpio_put(NANO_PWR_CTL_PIN, 1);

    // Prepare the radio connection
    spi_init(spi1, RADIO_SPI_BAUDRATE);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(RADIO_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RADIO_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RADIO_SCK_PIN, GPIO_FUNC_SPI);

    if (!rfm9x_init(&radio, spi1, RADIO_NCS_PIN, RADIO_RST_PIN, RADIO_FREQUENCY)) {
        panic("Radio initialization failed!");
    }

    rfm9x_set_spreading_factor(&radio, RADIO_SPREADING_FACTOR);
    rfm9x_set_signal_bandwidth(&radio, RADIO_SIGNAL_BANDWIDTH);
    rfm9x_set_coding_rate(&radio, RADIO_CODING_RATE);

    rfm9x_listen(&radio);

    sleep_ms(1000);
    safety_tick();

    if (!transport_can_init(CAN_BUS_CLIENT_ID)) {
        // No point in continuing onwards from here, if we can't initialize CAN
        // hardware might as well panic and retry
        panic("Failed to initialize CAN bus hardware!");
    }

    // Enter main loop
    // This is split into two sections of timers
    // Those running with ROS, and those in the background
    // Note that both types of timers will need to conform to the minimal delay
    // time, as there is around
    //   20ms of time worst case before the watchdog fires (as the ROS timeout is
    //   30ms)
    // Meaning, don't block, either poll it in the background task or send it to
    // an interrupt
    bool ros_initialized = false;
    while (true) {
        // Do background tasks
        tick_background_tasks();

        // Handle ROS state logic
        if (is_ros_connected()) {
            if (!ros_initialized) {
                LOG_INFO("ROS connected");

                // Lower all ROS related faults as we've got a new ROS context
                safety_lower_fault(FAULT_ROS_ERROR);

                if (ros_init(0) == RCL_RET_OK) {  // TODO supply board serial number here
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
        } else if (ros_initialized) {
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

        // handle radio traffic
        uint8_t packet_buffer[256];
        int received =
            rfm9x_receive(&radio, packet_buffer, sizeof(packet_buffer), true, false, false, RADIO_RX_TIMEOUT_MS);
        if (received > 0) {
            handle_radio_packets(received, packet_buffer);
        }

        // Tick safety
        safety_tick();
    }

    return 0;
}
