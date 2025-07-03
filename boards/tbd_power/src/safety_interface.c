#include <assert.h>
// #include "driver/canbus.h" CRH: USB transport is used instead of CAN
#include "driver/led.h"
#include "safety_interface.h"
#include "titan/logger.h"

remote_kill_switch_states_t remote_kill_switch_states[] = {
    REMOTE_KILL_SWITCH_DISABLED, REMOTE_KILL_SWITCH_DISABLED, REMOTE_KILL_SWITCH_DISABLED, REMOTE_KILL_SWITCH_DISABLED,
    REMOTE_KILL_SWITCH_DISABLED, REMOTE_KILL_SWITCH_DISABLED, REMOTE_KILL_SWITCH_DISABLED,
};

// ========================================
// Implementations for External Interface Functions
// ========================================

void safety_set_fault_led(bool on) {
#ifdef MICRO_ROS_TRANSPORT_CAN
    canbus_set_device_in_error(on);
#endif

    led_fault_set(on);
}

void safety_handle_kill(void) {
    // Note: Any calls made in this function must be safe to be called from interrupts
    // This is because safety_kill_switch_update can be called from interrupts

    // TODO: Modify this function to add callbacks when system is killed
    led_killswitch_set(false);
}

void safety_handle_enable(void) {
    // TODO: Modify this function to add callbacks for when system is enabled

    led_killswitch_set(true);
}

void safety_interface_setup(void) {}

// CRH: USB transport is used instead of CAN
// void safety_handle_can_internal_error(__unused canbus_error_data_t error_data) {
//     safety_raise_fault(FAULT_CAN_INTERNAL_ERROR);
// }

// void safety_handle_can_receive_error(__unused enum canbus_receive_error_codes err_code) {
//     //safety_raise_fault(FAULT_CAN_RECV_ERROR);
// }

void safety_interface_init(void) {
    // CRH: USB transport is used instead of CAN
    // canbus_set_receive_error_cb(safety_handle_can_receive_error);
    // canbus_set_internal_error_cb(safety_handle_can_internal_error);
}

void safety_interface_tick(void) {
    // TODO read the KS states in here
    // Read the onboard killswitch
    safety_kill_switch_update(0, false, true);

    // Update the offboard kill switches
    // Offboard starts at 1
    for (int i = 1; i < NUM_KILL_SWITCHES; i++) {
        if (remote_kill_switch_states[i] == REMOTE_KILL_SWITCH_ASSERTING) {
            LOG_INFO("Remote kill switch %d asserting", i);
            safety_kill_switch_update(i, true, true);
        } else if (remote_kill_switch_states[i] == REMOTE_KILL_SWITCH_NOT_ASSERTING) {
            safety_kill_switch_update(i, false, true);
        } else if (remote_kill_switch_states[i] == REMOTE_KILL_SWITCH_NO_CONTACT) {
            // If no contact, we assume the switch is not asserting
            safety_kill_switch_update(i, false, true);
        } else if (remote_kill_switch_states[i] == REMOTE_KILL_SWITCH_DISABLED) {
            // If disabled, we do not update the switch
            continue;
        }
        // safety_kill_switch_update(0, false, false);
    }
}

void safety_interface_deinit(void) {
    // TODO: Modify this function to add code to be called during safety_deinit
}

void set_radio_kill_switch_state(uint32_t switch_id, remote_kill_switch_states_t remote_kill_switch_state) {
    assert(switch_id < NUM_KILL_SWITCHES);
    assert(switch_id >= 0);
    remote_kill_switch_states[switch_id] = remote_kill_switch_state;
}

// ========================================
// Constant Calculations - Does not need to be modified
// ========================================

struct kill_switch_state kill_switch_states[NUM_KILL_SWITCHES] = { [0 ... NUM_KILL_SWITCHES - 1] = { .enabled =
                                                                                                         false } };
const int num_kill_switches = sizeof(kill_switch_states) / sizeof(*kill_switch_states);
static_assert(sizeof(kill_switch_states) / sizeof(*kill_switch_states) <= 32, "Too many kill switches defined");

const char* safety_lookup_killswitch_id(uint32_t switch_id) {
    assert(switch_id < sizeof(killswitch_id_list) / sizeof(*killswitch_id_list));
    return killswitch_id_list[switch_id];
}

const char* safety_lookup_fault_id(uint32_t fault_id) {
    return (fault_id < sizeof(fault_string_list) / sizeof(*fault_string_list) ? fault_string_list[fault_id]
                                                                              : "UNKNOWN");
}
