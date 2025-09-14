#ifndef ROS_H
#define ROS_H

#include <rcl/error_handling.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <rmw_microros/rmw_microros.h>

#include "safety_interface.h"

#define RCRETCHECK(fn)                                                                                \
    {                                                                                                 \
        rcl_ret_t temp_rc = fn;                                                                       \
        if ((temp_rc != RCL_RET_OK)) {                                                                \
            LOG_ERROR("Failed status on in " __FILE__ ":%d : %d. Aborting.", __LINE__, (int)temp_rc); \
            safety_raise_fault(FAULT_ROS_ERROR);                                                      \
            return temp_rc;                                                                           \
        }                                                                                             \
    }
#define RCSOFTRETCHECK(fn)                                                                              \
    {                                                                                                   \
        rcl_ret_t temp_rc = fn;                                                                         \
        if ((temp_rc != RCL_RET_OK)) {                                                                  \
            LOG_DEBUG("Failed status on in " __FILE__ ":%d : %d. Continuing.", __LINE__, (int)temp_rc); \
            return temp_rc;                                                                             \
        }                                                                                               \
    }
#define RCSOFTRETVCHECK(fn)                                                                             \
    {                                                                                                   \
        rcl_ret_t temp_rc = fn;                                                                         \
        if ((temp_rc != RCL_RET_OK)) {                                                                  \
            LOG_DEBUG("Failed status on in " __FILE__ ":%d : %d. Continuing.", __LINE__, (int)temp_rc); \
            return;                                                                                     \
        }                                                                                               \
    }
#define RCSOFTCHECK(fn)                                                                                 \
    {                                                                                                   \
        rcl_ret_t temp_rc = fn;                                                                         \
        if ((temp_rc != RCL_RET_OK)) {                                                                  \
            LOG_DEBUG("Failed status on in " __FILE__ ":%d : %d. Continuing.", __LINE__, (int)temp_rc); \
        }                                                                                               \
    }

// ========================================
// ROS Core Functions
// ========================================

/**
 * @brief Attempt to initialize ROS after a successful ping from the agent
 *
 * @param board_id The ID for the board
 * @return rcl_ret_t Return error code
 */
rcl_ret_t ros_init(uint8_t board_id);

/**
 * @brief Clean up a previously initialized or attempted initialized ROS connection
 *
 * @attention Ensure this is ALWAYS called after `ros_init` is called, whether or not it succeeds,
 * to avoid memory leaks.
 */
void ros_fini(void);

/**
 * @brief Spin the executor once and handle any incoming packets
 */
void ros_spin_executor(void);

/**
 * @brief Reports if ROS is connected, calculated based on if a heartbeat message successfully sends
 *
 * @return true ROS is still connected
 * @return false Enough heartbeats have failed that the ROS connection is considered dead
 */
bool is_ros_connected(void);

/**
 * @brief Attempt to ping the agent
 *
 * @return true Ping successful
 * @return false No response received from the agent
 */
bool ros_ping(void);

// ========================================
// ROS Task Functions
// ========================================

rcl_ret_t ros_heartbeat_pulse(uint8_t client_id);

rcl_ret_t ros_update_firmware_status(uint8_t client_id);

rcl_ret_t ros_update_killswitches(void);

/// @brief Publish electrical readings data
/// @param battery_1_voltage Battery 1 voltage reading
/// @param battery_2_voltage Battery 2 voltage reading  
/// @param input_voltage Input voltage reading
/// @param regulator_15v 15V regulator voltage reading
/// @param regulator_12v 12V regulator voltage reading
/// @param regulator_5v 5V regulator voltage reading
/// @param is_battery_1 Flag indicating if battery 1 is supplying power
/// @param is_battery_2 Flag indicating if battery 2 is supplying power
/// @return rcl_ret_t Return error code
rcl_ret_t ros_update_electrical_readings(float battery_1_voltage, float battery_2_voltage, 
                                        float input_voltage, float regulator_15v, 
                                        float regulator_12v, float regulator_5v,
                                        bool is_battery_1, bool is_battery_2);

void channel_restart_callback(const void* request_msg, void* response_msg);

static uint8_t channel_restart = 255;  // 255 means no channel restart requested

#endif
