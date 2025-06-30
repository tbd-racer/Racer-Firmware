#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include <rmw_microros/rmw_microros.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <chassis_msgs/msg/firmware_status.h>
#include <chassis_msgs/msg/killswitch_report.h>
#include <chassis_msgs/srv/restart_power_channel.h>
#include <std_msgs/msg/int8.h>

#include "titan/version.h"
#include "titan/logger.h"

#include "ros.h"

#undef LOGGING_UNIT_NAME
#define LOGGING_UNIT_NAME "ros"

// ========================================
// Global Definitions
// ========================================

#define MAX_MISSSED_HEARTBEATS 7
#define HEARTBEAT_PUBLISHER_NAME "state/fw_heartbeat"
#define FIRMWARE_STATUS_PUBLISHER_NAME "state/firmware"
#define KILLSWITCH_STATUS_PUBLISHER_NAME "state/kill"
#define CHANNEL_RESTART_SERVICE_NAME "state/restart"

bool ros_connected = false;

// Core Variables
rcl_node_t node;
rcl_allocator_t allocator;
rclc_support_t support;
rclc_executor_t executor;
rcl_publisher_t heartbeat_publisher;
int failed_heartbeats = 0;

// Node specific Variables
rcl_publisher_t firmware_status_publisher;
rcl_publisher_t killswitch_publisher;
rcl_service_t channel_restart_service;
chassis_msgs__srv__RestartPowerChannel_Request channel_restart_request_msg;
chassis_msgs__srv__RestartPowerChannel_Response channel_restart_response_msg;
// TODO: Add node specific items hereq

// ========================================
// Executor Callbacks
// ========================================

void channel_restart_callback(const void * request_msg, void * response_msg){
    // Cast messages to expected types
    chassis_msgs__srv__RestartPowerChannel_Request * req_in =
        (chassis_msgs__srv__RestartPowerChannel_Request *) request_msg;
    chassis_msgs__srv__RestartPowerChannel_Response * res_in =
        (chassis_msgs__srv__RestartPowerChannel_Response *) response_msg;

    // Handle request message and set the response message values
    uint8_t channel = req_in->channel_id;
    // If a channel restart is already requested, return an error
    if(channel_restart != 255){
        res_in->error_code = chassis_msgs__srv__RestartPowerChannel_Response__ERROR_CODE_BUSY;
        return;
    //  If the channel is invalid, return an error
    } else if(channel >= chassis_msgs__srv__RestartPowerChannel_Request__MAX_CHANNEL_ID) {
        res_in->error_code = chassis_msgs__srv__RestartPowerChannel_Response__ERROR_CODE_BAD;
        return;
    } else {
        channel_restart = channel;
        res_in->error_code = chassis_msgs__srv__RestartPowerChannel_Response__ERROR_CODE_OK;
    }
}

// ========================================
// Public Task Methods (called in main tick)
// ========================================

rcl_ret_t ros_update_firmware_status(uint8_t client_id) {
    chassis_msgs__msg__FirmwareStatus status_msg;
    status_msg.board_name.data = PICO_BOARD;
    status_msg.board_name.size = strlen(PICO_BOARD);
    status_msg.board_name.capacity = status_msg.board_name.size + 1; // includes NULL byte
    status_msg.bus_id = __CONCAT(CAN_BUS_NAME, _ID);
    status_msg.client_id = client_id;
    status_msg.uptime_ms = to_ms_since_boot(get_absolute_time());
    status_msg.version_major = MAJOR_VERSION;
    status_msg.version_minor = MINOR_VERSION;
    status_msg.version_release_type = RELEASE_TYPE;
    status_msg.faults = *fault_list_reg;

    RCSOFTRETCHECK(rcl_publish(&firmware_status_publisher, &status_msg, NULL));

    return RCL_RET_OK;
}

rcl_ret_t ros_heartbeat_pulse(uint8_t client_id) {
    std_msgs__msg__Int8 heartbeat_msg;
    heartbeat_msg.data = client_id;
    rcl_ret_t ret = rcl_publish(&heartbeat_publisher, &heartbeat_msg, NULL);
    if (ret != RCL_RET_OK) {
        failed_heartbeats++;

        if(failed_heartbeats > MAX_MISSSED_HEARTBEATS) {
            ros_connected = false;
        }
    } else {
        failed_heartbeats = 0;
    }

    RCSOFTRETCHECK(ret);

    return RCL_RET_OK;
}

rcl_ret_t ros_update_killswitches(void) {
    for(int i = 0; i < NUM_KILL_SWITCHES; i++){
        chassis_msgs__msg__KillswitchReport kill_msg;
        kill_msg.sender_id.data = killswitch_id_list[i];
        kill_msg.sender_id.size = strlen(killswitch_id_list[i]);
        kill_msg.sender_id.capacity = kill_msg.sender_id.size + 1; // includes NULL byte

        // Only ID 0 is physical, rest are remote
        if(i == 0){
            kill_msg.kill_switch_type = chassis_msgs__msg__KillswitchReport__KILL_SWITCH_TYPE_PHYSICAL;
        } else {
            kill_msg.kill_switch_type = chassis_msgs__msg__KillswitchReport__KILL_SWITCH_TYPE_REMOTE;
        }
        kill_msg.needs_heartbeat = kill_switch_states[i].needs_update;
        kill_msg.switch_asserting_kill = kill_switch_states[i].asserting_kill;

        RCSOFTRETCHECK(rcl_publish(&killswitch_publisher, &kill_msg, NULL));
    }

    return RCL_RET_OK;
}

// ========================================
// ROS Core
// ========================================

char node_name[sizeof(PICO_TARGET_NAME) + 6];

rcl_ret_t ros_init(uint8_t board_id) {
    // ROS Core Initialization
    allocator = rcl_get_default_allocator();
    RCRETCHECK(rclc_support_init(&support, 0, NULL, &allocator));

    snprintf(node_name, sizeof(node_name), PICO_TARGET_NAME "_%d", board_id);
    RCRETCHECK(rclc_node_init_default(&node, node_name, "", &support));

    // Node Initialization
    RCRETCHECK(rclc_publisher_init_default(
        &heartbeat_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int8),
        HEARTBEAT_PUBLISHER_NAME));

    RCRETCHECK(rclc_publisher_init_default(
        &firmware_status_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(chassis_msgs, msg, FirmwareStatus),
        FIRMWARE_STATUS_PUBLISHER_NAME));

    RCRETCHECK(rclc_publisher_init_default(
        &killswitch_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(chassis_msgs, msg, KillswitchReport),
        KILLSWITCH_STATUS_PUBLISHER_NAME));

    RCRETCHECK(rclc_service_init_default(
        &channel_restart_service,
        &node,
        ROSIDL_GET_SRV_TYPE_SUPPORT(chassis_msgs, srv, RestartPowerChannel),
        CHANNEL_RESTART_SERVICE_NAME));

    // Executor Initialization
    const int executor_num_handles = 2;
    RCRETCHECK(rclc_executor_init(&executor, &support.context, executor_num_handles, &allocator));
    RCRETCHECK(rclc_executor_add_service(&executor, &channel_restart_service, &channel_restart_request_msg,
  &channel_restart_response_msg, channel_restart_callback));

    // Note: Code in executor callbacks should be kept to a minimum
    // It should set whatever flags are necessary and get out
    // And it should *NOT* try to perform any communications over ROS, as this can lead to watchdog timeouts
    // in the event that specific request times out

    return RCL_RET_OK;
}

void ros_spin_executor(void) {
    rclc_executor_spin_some(&executor, 0);
}

void ros_fini(void) {
    RCSOFTCHECK(rcl_publisher_fini(&heartbeat_publisher, &node));
    RCSOFTCHECK(rcl_publisher_fini(&firmware_status_publisher, &node));
    RCSOFTCHECK(rcl_publisher_fini(&killswitch_publisher, &node));
    RCSOFTCHECK(rcl_service_fini(&channel_restart_service, &node));
    RCSOFTCHECK(rclc_executor_fini(&executor));
    RCSOFTCHECK(rcl_node_fini(&node));
    RCSOFTCHECK(rclc_support_fini(&support));

    ros_connected = false;
}

bool is_ros_connected(void) {
    return ros_connected;
}

bool ros_ping(void) {
    ros_connected = rmw_uros_ping_agent(RMW_UXRCE_PUBLISH_RELIABLE_TIMEOUT, 1) == RCL_RET_OK;
    return ros_connected;
}