// =============================================================================
// step_back.h - Step-backward execution system using state snapshots.
// =============================================================================
//
//  This module provides "undo" functionality for the debugger, allowing users
//  to step backward through program execution. It works by maintaining a ring
//  buffer of VM state snapshots.
//
//  Architecture:
//    * VMStateSnapshot - Complete snapshot of VM state at a point in time
//    * StepBackBuffer  - Circular buffer managing snapshot history
//    * StepBackSystem  - Facade for recording and restoring states
// =============================================================================

#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>

// Forward declaration
class VM;

// =============================================================================
//  VM State Snapshot
// =============================================================================

struct VMStateSnapshot {
    uint8_t memory[16];       // Complete memory state
    uint8_t pc;               // Program counter
    uint8_t accumulator;      // Accumulator register
    uint8_t output_reg;       // Output register
    bool halted;              // Halted flag
    bool carry_flag;          // Carry flag
    bool zero_flag;           // Zero flag
    
    // Execution metadata
    int step_number;          // Which step this snapshot represents
    double timestamp;         // When this snapshot was taken
    
    VMStateSnapshot() {
        std::fill(std::begin(memory), std::end(memory), 0);
        pc = 0;
        accumulator = 0;
        output_reg = 0;
        halted = false;
        carry_flag = false;
        zero_flag = false;
        step_number = 0;
        timestamp = 0.0;
    }
    
    // Capture current VM state
    void capture_from(const VM& vm);
    
    // Restore VM state from this snapshot
    void restore_to(VM& vm) const;
};

// =============================================================================
//  Step-Back Buffer (Circular Buffer)
// =============================================================================

class StepBackBuffer {
public:
    static constexpr size_t DEFAULT_CAPACITY = 64;  // Max steps to remember
    
    explicit StepBackBuffer(size_t capacity = DEFAULT_CAPACITY);
    
    // Record current VM state
    void record_state(const VM& vm, int step_number);
    
    // Get the previous state (returns nullptr if none available)
    const VMStateSnapshot* get_previous() const;
    
    // Get state N steps back (returns nullptr if out of range)
    const VMStateSnapshot* get_state_n_steps_back(int n) const;
    
    // Check if we can step back
    bool can_step_back() const { return count_ > 0 && current_index_ > 0; }
    
    // Check if we can step forward
    bool can_step_forward() const { return count_ > 1 && current_index_ < count_; }
    
    // Move the current position back one step
    // Returns true if successful
    bool step_back();
    
    // Move the current position forward one step
    // Returns true if successful
    bool step_forward();
    
    // Reset to the most recent state (discard undo history)
    void reset();
    
    // Get number of available backward steps
    size_t available_backward_steps() const { return current_index_; }
    
    // Get total recorded states
    size_t total_states() const { return count_; }
    
    // Get capacity
    size_t capacity() const { return capacity_; }

private:
    std::vector<VMStateSnapshot> snapshots_;
    size_t capacity_;
    size_t count_;           // Number of valid snapshots
    size_t current_index_;   // Current position in history (1-based, 0 = initial)
};

// =============================================================================
//  Step-Back System Facade
// =============================================================================

class StepBackSystem {
public:
    StepBackSystem();
    
    // Enable/disable state recording
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }
    
    // Record current VM state (call after each step)
    void record_state(const VM& vm);
    
    // Try to step back one instruction
    // Returns true if successful and restores the previous state
    bool step_back(VM& vm);
    
    // Try to step forward one instruction
    // Returns true if successful and restores the next state
    bool step_forward(VM& vm);
    
    // Reset the step-back history
    void reset();
    
    // Get available backward steps
    size_t available_backward_steps() const;
    
    // Get the last recorded step number
    int last_step_number() const { return last_step_number_; }

private:
    StepBackBuffer buffer_;
    bool enabled_;
    int last_step_number_;
    int total_steps_executed_;  // Total steps since last reset
};

// =============================================================================
//  Global Accessor
// =============================================================================

// Get the global step-back system instance
StepBackSystem& get_step_back_system();
