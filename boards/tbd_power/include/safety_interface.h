#ifndef SAFETY_INTERFACE_H
#define SAFETY_INTERFACE_H

#include "titan/safety.h"

// NOTE: If adding fault IDs make sure to update the fault_string_list as well

//      FAULT_WATCHDOG_RESET      0
//      FAULT_WATCHDOG_WARNING    1
#define FAULT_CAN_INTERNAL_ERROR  2
#define FAULT_CAN_RECV_ERROR      3
#define FAULT_ROS_ERROR           4
#define FAULT_TIMER_MISSED        5
#define FAULT_RFM95_ERROR         6

static const char * const fault_string_list[] = {
    "FAULT_WATCHDOG_RESET",
    "FAULT_WATCHDOG_WARNING",
    "FAULT_CAN_INTERNAL_ERROR",
    "FAULT_CAN_RECV_ERROR",
    "FAULT_ROS_ERROR",
    "FAULT_TIMER_MISSED",
    "FAULT_RFM95_ERROR"
};

const char * safety_lookup_killswitch_id(uint32_t switch_id);

// Map for killswitch names
static const char * const killswitch_id_list[] = {
    "ONBOARD_0",
    "RADIO_REMOTE_0",
    "RADIO_REMOTE_1",
    "RADIO_REMOTE_2",
    "RADIO_REMOTE_3",
    "RADIO_REMOTE_4",
    "RADIO_REMOTE_5",
};

// TODO: Replace these with the kill switches for the implementation
// If no kill switches defined, set NUM_KILL_SWITCHES = 0
enum kill_switch {
    KILL_SWITCH_PHYSICAL = 0,
    KILL_SWITCH_RADIO_0,
    KILL_SWITCH_RADIO_1,
    KILL_SWITCH_RADIO_2,
    KILL_SWITCH_RADIO_3,
    KILL_SWITCH_RADIO_4,
    KILL_SWITCH_RADIO_5,
    

    // Used to automatically calculate number of kill switches
    // This must be the last enum
    NUM_KILL_SWITCHES
};

typedef enum rm_ks_states {
    REMOTE_KILL_SWITCH_NO_CONTACT,
    REMOTE_KILL_SWITCH_ASSERTING,
    REMOTE_KILL_SWITCH_NOT_ASSERTING,
    REMOTE_KILL_SWITCH_DISABLED
} remote_kill_switch_states_t;

void set_radio_kill_switch_state(uint32_t switch_id, remote_kill_switch_states_t remote_kill_switch_state);



#endif