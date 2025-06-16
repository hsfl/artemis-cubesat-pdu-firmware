/**
 * @file state_machine.h
 * @brief Power Distribution Unit (PDU) State Machine Implementation
 * 
 * This state machine controls the operational states of the PDU:
 * - OFF: Initial state, minimal power consumption
 * - INIT: System initialization and self-test
 * - SAFE: Normal operation with all safety checks active
 * - STANDBY: Low-power mode with minimal functionality
 * - NOMINAL: Full operational mode
 * - EMERGENCY: Emergency power management mode
 * - DIAGNOSTIC: System diagnostic mode TODO
 * - SHUTDOWN: Controlled shutdown sequence
 * 
 * Usage:
 * 1. Create a state machine instance:
 *    @code
 *    pdu_state_machine_t sm;
 *    pdu_state_machine_init(&sm);
 *    @endcode
 * 
 * 2. Process events in your main loop:
 *    @code
 *    pdu_state_machine_process_event(&sm, PDU_EVENT_POWER_ON);
 *    @endcode
 * 
 * 3. Check current state when needed:
 *    @code
 *    pdu_state_t current_state = pdu_state_machine_get_state(&sm);
 *    @endcode
 * 
 * Modifying the State Machine:
 * 1. To add a new state:
 *    - Add it to the pdu_state_t enum
 *    - Add a new entry action function in state_machine.c
 *    - Add a new row in the state_transitions table
 * 
 * 2. To add a new event:
 *    - Add it to the pdu_event_t enum
 *    - Add a new column in the state_transitions table
 *    - Update all state transition handlers
 * 
 * 3. To modify state transitions:
 *    - Update the state_transitions table in state_machine.c
 *    - Add/remove entry actions as needed
 * 
 * Compile-time Tracing:
 * Define PDU_STATE_MACHINE_TRACE in your build configuration to enable
 * state transition tracing. Implement the actual tracing mechanism in
 * the PDU_STATE_TRACE macro.
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>

/**
 * @brief PDU operational states
 * 
 * OFF: Initial state, minimal power consumption
 * INIT: System initialization and self-test
 * SAFE: Normal operation with all safety checks active
 * STANDBY: Low-power mode with minimal functionality
 * NOMINAL: Full operational mode
 * EMERGENCY: Emergency power management mode
 * DIAGNOSTIC: System diagnostic mode
 * SHUTDOWN: Controlled shutdown sequence
 */
typedef enum {
    PDU_STATE_OFF = 0,    ///< Initial state, minimal power consumption
    PDU_STATE_INIT,       ///< System initialization and self-test
    PDU_STATE_SAFE,       ///< Normal operation with all safety checks active
    PDU_STATE_STANDBY,    ///< Low-power mode with minimal functionality
    PDU_STATE_NOMINAL,    ///< Full operational mode
    PDU_STATE_EMERGENCY,  ///< Emergency power management mode
    PDU_STATE_DIAGNOSTIC, ///< System diagnostic mode
    PDU_STATE_SHUTDOWN,   ///< Controlled shutdown sequence
    PDU_STATE_COUNT       ///< Number of states (for bounds checking)
} pdu_state_t;

/**
 * @brief Events that trigger state transitions
 * 
 * POWER_ON: System power-up request
 * INIT_COMPLETE: Initialization sequence completed successfully
 * ERROR: Error condition detected
 * POWER_OFF: System shutdown request
 */
typedef enum {
    PDU_EVENT_POWER_ON = 0,    ///< System power-up request
    PDU_EVENT_INIT_COMPLETE,   ///< Initialization sequence completed successfully
    PDU_EVENT_ERROR,           ///< Error condition detected
    PDU_EVENT_POWER_OFF,       ///< System shutdown request
    PDU_EVENT_COUNT            ///< Number of events (for bounds checking)
} pdu_event_t;

/**
 * @brief State machine context structure
 * 
 * Holds the current state and timing information
 */
typedef struct {
    pdu_state_t current_state;     ///< Current operational state
    uint32_t state_entry_time_ms;  ///< Timestamp when current state was entered
} pdu_state_machine_t;

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
void pdu_state_machine_init(pdu_state_machine_t *sm);

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
void pdu_state_machine_process_event(pdu_state_machine_t *sm, pdu_event_t event);

/**
 * @brief Get the current state
 * 
 * @param sm Pointer to state machine context
 * @return Current state (PDU_STATE_OFF if sm is NULL)
 */
pdu_state_t pdu_state_machine_get_state(const pdu_state_machine_t *sm);

/* Compile-time tracing macros */
#ifdef PDU_STATE_MACHINE_TRACE
    #define PDU_STATE_TRACE(state) do { \
        /* Add your trace implementation here */ \
    } while(0)
#else
    #define PDU_STATE_TRACE(state) do {} while(0)
#endif

#endif /* STATE_MACHINE_H */ 