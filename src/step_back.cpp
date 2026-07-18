// =============================================================================
// step_back.cpp - Step-backward execution system implementation.
// =============================================================================

#include "step_back.h"
#include "vm.h"

#include <cstring>
#include <chrono>

// =============================================================================
//  VMStateSnapshot Implementation
// =============================================================================

void VMStateSnapshot::capture_from(const VM& vm) {
    std::memcpy(memory, vm.memory, sizeof(memory));
    pc = vm.pc;
    accumulator = vm.accumulator;
    output_reg = vm.output_reg;
    halted = vm.halted;
    carry_flag = vm.carry_flag;
    zero_flag = vm.zero_flag;
    
    // Use high-resolution clock for precise timestamps
    auto now = std::chrono::steady_clock::now();
    timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
}

void VMStateSnapshot::restore_to(VM& vm) const {
    std::memcpy(vm.memory, memory, sizeof(vm.memory));
    vm.pc = pc;
    vm.accumulator = accumulator;
    vm.output_reg = output_reg;
    vm.halted = halted;
    vm.carry_flag = carry_flag;
    vm.zero_flag = zero_flag;
    // Note: console_output is not restored to avoid confusion
}

// =============================================================================
//  StepBackBuffer Implementation
// =============================================================================

StepBackBuffer::StepBackBuffer(size_t capacity)
    : snapshots_(capacity)
    , capacity_(capacity)
    , count_(0)
    , current_index_(0) {
}

void StepBackBuffer::record_state(const VM& vm, int step_number) {
    if (snapshots_.empty()) return;
    
    // If we've stepped forward and then record a new state,
    // discard the forward history
    if (current_index_ < count_) {
        count_ = current_index_;
    }
    
    // Add new snapshot at current position
    size_t insert_index = current_index_;
    if (insert_index >= capacity_) {
        // Buffer is full, shift everything back
        for (size_t i = 1; i < capacity_; ++i) {
            snapshots_[i - 1] = snapshots_[i];
        }
        insert_index = capacity_ - 1;
        current_index_ = insert_index;
        // Adjust step numbers for display purposes
        for (size_t i = 0; i < capacity_ - 1; ++i) {
            snapshots_[i].step_number = step_number - static_cast<int>(capacity_ - 1 - i);
        }
    } else {
        ++current_index_;
    }
    
    snapshots_[insert_index].capture_from(vm);
    snapshots_[insert_index].step_number = step_number;
    
    if (current_index_ > count_) {
        count_ = current_index_;
    }
}

const VMStateSnapshot* StepBackBuffer::get_previous() const {
    if (current_index_ == 0 || current_index_ > count_) {
        return nullptr;
    }
    return &snapshots_[current_index_ - 1];
}

const VMStateSnapshot* StepBackBuffer::get_state_n_steps_back(int n) const {
    if (n <= 0 || static_cast<size_t>(n) >= current_index_) {
        return nullptr;
    }
    return &snapshots_[current_index_ - n];
}

bool StepBackBuffer::step_back() {
    if (current_index_ > 1) {
        --current_index_;
        return true;
    }
    return false;
}

bool StepBackBuffer::step_forward() {
    if (current_index_ < count_) {
        ++current_index_;
        return true;
    }
    return false;
}

void StepBackBuffer::reset() {
    count_ = 0;
    current_index_ = 0;
}

// =============================================================================
//  StepBackSystem Implementation
// =============================================================================

StepBackSystem::StepBackSystem()
    : buffer_()
    , enabled_(true)
    , last_step_number_(0)
    , total_steps_executed_(0) {
}

void StepBackSystem::record_state(const VM& vm) {
    if (!enabled_) return;
    
    ++total_steps_executed_;
    last_step_number_ = total_steps_executed_;
    
    buffer_.record_state(vm, last_step_number_);
}

bool StepBackSystem::step_back(VM& vm) {
    if (!enabled_) return false;
    
    const VMStateSnapshot* prev = buffer_.get_previous();
    if (!prev) return false;
    
    buffer_.step_back();
    prev->restore_to(vm);
    return true;
}

bool StepBackSystem::step_forward(VM& vm) {
    if (!enabled_) return false;
    
    if (!buffer_.step_forward()) return false;
    
    // Get the state at the new position
    const VMStateSnapshot* next = buffer_.get_state_n_steps_back(
        static_cast<int>(buffer_.available_backward_steps()));
    if (next) {
        next->restore_to(vm);
        return true;
    }
    return false;
}

void StepBackSystem::reset() {
    buffer_.reset();
    last_step_number_ = 0;
    total_steps_executed_ = 0;
}

size_t StepBackSystem::available_backward_steps() const {
    return buffer_.available_backward_steps();
}

// =============================================================================
//  Global Instance
// =============================================================================

static StepBackSystem g_step_back_system;

StepBackSystem& get_step_back_system() {
    return g_step_back_system;
}
