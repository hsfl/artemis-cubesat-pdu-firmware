/**
 * @file state_machine.c
 * @brief Power Distribution Unit (PDU) State Machine Implementation
 * 
 * This file implements the state machine logic for the PDU. The state machine
 * uses a transition table approach for efficient state transitions and clear
 * state transition rules.
 * 
 * Implementation Details:
 * 1. State Transition Table:
 *    - 2D array indexed by [current_state][event]
 *    - Each entry contains next state and entry action
 *    - NULL entry actions mean no action needed
 * 
 * 2. State Entry Actions:
 *    - Each state has a dedicated entry function
 *    - Entry functions are called after state transitions
 *    - Add state-specific initialization here
 * 
 * 3. Error Handling:
 *    - NULL pointer checks
 *    - Bounds checking for states and events
 *    - Safe default behavior (OFF state)
 * 
 * Modifying the Implementation:
 * 1. Adding a New State:
 *    a. Add state entry function:
 *       @code
 *       static void state_new_entry(pdu_state_machine_t *sm)
 *       {
 *           PDU_STATE_TRACE(PDU_STATE_NEW);
 *           // Add state-specific initialization
 *       }
 *       @endcode
 * 
 *    b. Add state transitions in state_transitions table:
 *       @code
 *       [PDU_STATE_NEW] = {
 *           [PDU_EVENT_POWER_ON] = {PDU_STATE_NEW, NULL},
 *           // Add other event transitions
 *       },
 *       @endcode
 * 
 * 2. Adding State-Specific Logic:
 *    - Add to the appropriate state entry function
 *    - Consider adding state exit actions if needed
 *    - Add state-specific data to pdu_state_machine_t if required
 * 
 * 3. Modifying State Transitions:
 *    - Update the state_transitions table
 *    - Add/remove entry actions as needed
 *    - Update state entry functions if behavior changes
 */

#include "state_machine.h"
#include <string.h>
#include "boot_record.h"

/* State transition table type */
typedef struct {
    pdu_state_t next_state;                    ///< Next state after transition
    void (*entry_action)(pdu_state_machine_t *sm);  ///< Function to call on state entry
} state_transition_t;

/* Forward declarations of state entry actions */
static void state_off_entry(pdu_state_machine_t *sm);
static void state_init_entry(pdu_state_machine_t *sm);
static void state_safe_entry(pdu_state_machine_t *sm);
static void state_standby_entry(pdu_state_machine_t *sm);
static void state_nominal_entry(pdu_state_machine_t *sm);
static void state_emergency_entry(pdu_state_machine_t *sm);
//static void state_diagnostic_entry(pdu_state_machine_t *sm); // TODO: add DIAGNOSTIC state entry actions when it'll be used - maybe mission ops command/obc?
static void state_shutdown_entry(pdu_state_machine_t *sm);

/**
 * @brief State transition table
 * 
 * Defines all possible state transitions and their associated actions.
 * Table is indexed by [current_state][event].
 * 
 * State Transition Rules:
 * 
 * OFF State:
 * - POWER_ON → INIT: Initial system startup
 * - All other events maintain OFF state
 * 
 * INIT State:
 * - INIT_COMPLETE → SAFE: Successful initialization
 * - ERROR/POWER_OFF → OFF: Abort initialization
 * 
 * SAFE State:
 * - All events maintain SAFE state except:
 * - ERROR/POWER_OFF → OFF: System shutdown
 * 
 * STANDBY State:
 * - POWER_ON → NOMINAL: Resume normal operation
 * - ERROR → SAFE: Fallback to safe mode
 * - POWER_OFF → SHUTDOWN: Controlled shutdown
 * 
 * NOMINAL State:
 * - ERROR → EMERGENCY: Critical error handling
 * - POWER_OFF → SHUTDOWN: Controlled shutdown
 * - Other events maintain NOMINAL state
 * 
 * EMERGENCY State:
 * - ERROR → SAFE: Fallback to safe mode
 * - POWER_OFF → SHUTDOWN: Controlled shutdown
 * - Other events maintain EMERGENCY state
 * 
 * DIAGNOSTIC State:
 * - INIT_COMPLETE → SAFE: Return to safe mode
 * - ERROR → EMERGENCY: Escalate to emergency
 * - POWER_OFF → SHUTDOWN: Controlled shutdown
 * - Other events maintain DIAGNOSTIC state
 * 
 * SHUTDOWN State:
 * - ERROR/POWER_OFF → OFF: Complete shutdown
 * - Other events maintain SHUTDOWN state
 * 
 * General Rules:
 * 1. Error conditions generally lead to safer states
 * 2. Power-off requests follow controlled shutdown sequence
 * 3. State entry actions are called after transitions
 * 4. NULL entry actions indicate no special handling needed
 */
static const state_transition_t state_transitions[PDU_STATE_COUNT][PDU_EVENT_COUNT] = {
    /* PDU_STATE_OFF */
    [PDU_STATE_OFF] = {
        [PDU_EVENT_POWER_ON] = {PDU_STATE_INIT, state_init_entry},
        [PDU_EVENT_INIT_COMPLETE] = {PDU_STATE_OFF, NULL},
        [PDU_EVENT_ERROR] = {PDU_STATE_OFF, NULL},
        [PDU_EVENT_POWER_OFF] = {PDU_STATE_OFF, NULL},
    },
    /* PDU_STATE_INIT */
    [PDU_STATE_INIT] = {
        [PDU_EVENT_POWER_ON] = {PDU_STATE_INIT, NULL},
        [PDU_EVENT_INIT_COMPLETE] = {PDU_STATE_SAFE, state_safe_entry},
        [PDU_EVENT_ERROR] = {PDU_STATE_OFF, state_off_entry},
        [PDU_EVENT_POWER_OFF] = {PDU_STATE_OFF, state_off_entry},
    },
    /* PDU_STATE_SAFE */
    [PDU_STATE_SAFE] = {
        [PDU_EVENT_POWER_ON] = {PDU_STATE_STANDBY, state_standby_entry},
        [PDU_EVENT_INIT_COMPLETE] = {PDU_STATE_SAFE, NULL},
        [PDU_EVENT_ERROR] = {PDU_STATE_OFF, state_off_entry},
        [PDU_EVENT_POWER_OFF] = {PDU_STATE_OFF, state_off_entry},
    },
    /* PDU_STATE_STANDBY */
    [PDU_STATE_STANDBY] = {
        [PDU_EVENT_POWER_ON] = {PDU_STATE_NOMINAL, state_nominal_entry},
        [PDU_EVENT_INIT_COMPLETE] = {PDU_STATE_STANDBY, NULL},
        [PDU_EVENT_ERROR] = {PDU_STATE_SAFE, state_safe_entry},
        [PDU_EVENT_POWER_OFF] = {PDU_STATE_SHUTDOWN, state_shutdown_entry},
    },
    /* PDU_STATE_NOMINAL */
    [PDU_STATE_NOMINAL] = {
        [PDU_EVENT_POWER_ON] = {PDU_STATE_NOMINAL, NULL},
        [PDU_EVENT_INIT_COMPLETE] = {PDU_STATE_NOMINAL, NULL},
        [PDU_EVENT_ERROR] = {PDU_STATE_EMERGENCY, state_emergency_entry},
        [PDU_EVENT_POWER_OFF] = {PDU_STATE_SHUTDOWN, state_shutdown_entry},
    },
    /* PDU_STATE_EMERGENCY */
    [PDU_STATE_EMERGENCY] = {
        [PDU_EVENT_POWER_ON] = {PDU_STATE_EMERGENCY, NULL},
        [PDU_EVENT_INIT_COMPLETE] = {PDU_STATE_EMERGENCY, NULL},
        [PDU_EVENT_ERROR] = {PDU_STATE_SAFE, state_safe_entry},
        [PDU_EVENT_POWER_OFF] = {PDU_STATE_SHUTDOWN, state_shutdown_entry},
    },
    /* PDU_STATE_DIAGNOSTIC - TODO only allow transition if its a user command?*/
    [PDU_STATE_DIAGNOSTIC] = {
        [PDU_EVENT_POWER_ON] = {PDU_STATE_DIAGNOSTIC, NULL},
        [PDU_EVENT_INIT_COMPLETE] = {PDU_STATE_SAFE, state_safe_entry},
        [PDU_EVENT_ERROR] = {PDU_STATE_EMERGENCY, state_emergency_entry},
        [PDU_EVENT_POWER_OFF] = {PDU_STATE_SHUTDOWN, state_shutdown_entry},
    },
    /* PDU_STATE_SHUTDOWN */
    [PDU_STATE_SHUTDOWN] = {
        [PDU_EVENT_POWER_ON] = {PDU_STATE_SHUTDOWN, NULL},
        [PDU_EVENT_INIT_COMPLETE] = {PDU_STATE_SHUTDOWN, NULL},
        [PDU_EVENT_ERROR] = {PDU_STATE_OFF, state_off_entry},
        [PDU_EVENT_POWER_OFF] = {PDU_STATE_OFF, state_off_entry},
    },
};

/**
 * @brief OFF state entry action
 * 
 * Called when entering the OFF state. Performs:
 * - State transition tracing
 * - Power-down sequence
 * - Safety checks
 */
static void state_off_entry(pdu_state_machine_t *sm)
{
    PDU_STATE_TRACE(PDU_STATE_OFF);
    /* Add OFF state entry actions here */
}

/**
 * @brief INIT state entry action
 * 
 * Called when entering the INIT state. Performs:
 * - State transition tracing
 * - System initialization
 * - Self-test sequence
 */
static void state_init_entry(pdu_state_machine_t *sm)
{
    PDU_STATE_TRACE(PDU_STATE_INIT);
    Bootrec_Init();
    extern bootrec_t mirror; // defined in boot_record.c
    if (mirror.safe_latch) {
        // Immediately transition to SAFE state if latch is set
        sm->current_state = PDU_STATE_SAFE;
        state_safe_entry(sm);
        return;
    }
    /* Add INIT state entry actions here */
}

/**
 * @brief SAFE state entry action
 * 
 * Called when entering the SAFE state. Performs:
 * - State transition tracing
 * - Safety system activation
 * - Normal operation initialization
 */
static void state_safe_entry(pdu_state_machine_t *sm)
{
    PDU_STATE_TRACE(PDU_STATE_SAFE);
    /* Add SAFE state entry actions here */
}

/**
 * @brief STANDBY state entry action
 * 
 * Called when entering the STANDBY state. Performs:
 * - State transition tracing
 * - Low-power mode initialization
 */
static void state_standby_entry(pdu_state_machine_t *sm)
{
    PDU_STATE_TRACE(PDU_STATE_STANDBY);
    /* TODO: add STANDBY state entry actions */
}

/**
 * @brief NOMINAL state entry action
 * 
 * Called when entering the NOMINAL state. Performs:
 * - State transition tracing
 * - Full operation initialization
 */
static void state_nominal_entry(pdu_state_machine_t *sm)
{
    PDU_STATE_TRACE(PDU_STATE_NOMINAL);
    /* TODO: add NOMINAL state entry actions */
}

/**
 * @brief EMERGENCY state entry action
 * 
 * Called when entering the EMERGENCY state. Performs:
 * - State transition tracing
 * - Emergency power management initialization
 */
static void state_emergency_entry(pdu_state_machine_t *sm)
{
    PDU_STATE_TRACE(PDU_STATE_EMERGENCY);
    /* TODO: add EMERGENCY state entry actions */
}

/**
 * @brief DIAGNOSTIC state entry action
 * 
 * Called when entering the DIAGNOSTIC state. Performs:
 * - State transition tracing
 * - Diagnostic mode initialization
 */
// static void state_diagnostic_entry(pdu_state_machine_t *sm)
// {
//     TODO: add DIAGNOSTIC state entry actions when it'll be used - maybe mission ops command/obc?
//     PDU_STATE_TRACE(PDU_STATE_DIAGNOSTIC);
//     /* TODO: add DIAGNOSTIC state entry actions */
// }

/**
 * @brief SHUTDOWN state entry action
 * 
 * Called when entering the SHUTDOWN state. Performs:
 * - State transition tracing
 * - Controlled shutdown sequence
 */
static void state_shutdown_entry(pdu_state_machine_t *sm)
{
    PDU_STATE_TRACE(PDU_STATE_SHUTDOWN);
    /* TODO: add SHUTDOWN state entry actions */
}

/**
 * @brief Initialize the state machine
 * 
 * @param sm Pointer to state machine context
 * 
 * This function:
 * 1. Clears the state machine context
 * 2. Sets initial state to OFF
 * 3. Calls the OFF state entry action
 */
void pdu_state_machine_init(pdu_state_machine_t *sm)
{
    if (sm == NULL) {
        return;
    }
    
    memset(sm, 0, sizeof(pdu_state_machine_t));
    sm->current_state = PDU_STATE_OFF;
    state_off_entry(sm);
}

/**
 * @brief Process a state machine event
 * 
 * @param sm Pointer to state machine context
 * @param event Event to process
 * 
 * This function:
 * 1. Validates the event
 * 2. Looks up the next state in the transition table
 * 3. Performs the state transition if needed
 * 4. Calls the new state's entry action
 */
void pdu_state_machine_process_event(pdu_state_machine_t *sm, pdu_event_t event)
{
    if (sm == NULL || event >= PDU_EVENT_COUNT) {
        return;
    }

    const state_transition_t *transition = &state_transitions[sm->current_state][event];
    
    if (transition->next_state != sm->current_state) {
        sm->current_state = transition->next_state;
        if (transition->entry_action != NULL) {
            transition->entry_action(sm);
        }
    }
}

/**
 * @brief Get the current state
 * 
 * @param sm Pointer to state machine context
 * @return Current state (PDU_STATE_OFF if sm is NULL)
 */
pdu_state_t pdu_state_machine_get_state(const pdu_state_machine_t *sm)
{
    if (sm == NULL) {
        return PDU_STATE_OFF;
    }
    return sm->current_state;
} 