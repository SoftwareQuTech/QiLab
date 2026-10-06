// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <thread>
#include "logger/logger_api.hpp"
namespace thread_utils {

#define THREAD_SCHEDULER_FIFO_PRIORITY 80

inline pid_t get_current_tid() {
    return static_cast<pid_t>(syscall(SYS_gettid));
}

inline bool pin_current_thread_to_core(int core_id) {
    if (core_id < 0) return false;

    // Get current thread info
    pid_t tid = get_current_tid();
    
    // Set affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    
    bool success = true;
    
    // Try setting affinity
    if (sched_setaffinity(tid, sizeof(cpu_set_t), &cpuset) != 0) 
    {
        LOG_ERROR("Failed to set thread " + std::to_string(tid) + " affinity to core " + std::to_string(core_id) 
                  + " (errno: " + std::to_string(errno) + ")");
        success = false;
    }
    
    // Verify affinity
    CPU_ZERO(&cpuset);
    if (sched_getaffinity(tid, sizeof(cpu_set_t), &cpuset) == 0) 
    {
        if (!CPU_ISSET(core_id, &cpuset)) 
        {
            LOG_ERROR("Core " + std::to_string(core_id) + " not in affinity set for thread " + std::to_string(tid));
            success = false;
        }
    }
    
    // Set real-time priority
    struct sched_param param;
    param.sched_priority = THREAD_SCHEDULER_FIFO_PRIORITY;//sched_get_priority_max(SCHED_FIFO);
    
    if (sched_setscheduler(tid, SCHED_FIFO, &param) != 0) 
    {
        LOG_ERROR("Failed to set SCHED_FIFO for thread " + std::to_string(tid) 
                  + " (errno: " + std::to_string(errno) + ")");
        success = false;
    }

    // Get and print current core
    int current_core = sched_getcpu();
    LOG_INFO("Thread " + std::to_string(tid) + " requested core " + std::to_string(core_id) 
              + ", currently on core " + std::to_string(current_core) 
              + " (success: " + (success ? "true" : "false") + ")");
              
    // Verify we're on the right core
    if (current_core != core_id) 
    {
        LOG_ERROR("Thread " + std::to_string(tid) + " running on wrong core: " 
                  + std::to_string(current_core) + " instead of " + std::to_string(core_id));
        success = false;
    }

    return success;
}

} // namespace thread_utils