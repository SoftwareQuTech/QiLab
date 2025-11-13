#pragma once

#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <thread>

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
    if (sched_setaffinity(tid, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "Failed to set thread " << tid << " affinity to core " << core_id 
                  << " (errno: " << errno << ")" << std::endl;
        success = false;
    }
    
    // Verify affinity
    CPU_ZERO(&cpuset);
    if (sched_getaffinity(tid, sizeof(cpu_set_t), &cpuset) == 0) {
        if (!CPU_ISSET(core_id, &cpuset)) {
            std::cerr << "Core " << core_id << " not in affinity set for thread " << tid << std::endl;
            success = false;
        }
    }
    
    // Set real-time priority
    struct sched_param param;
    param.sched_priority = THREAD_SCHEDULER_FIFO_PRIORITY;//sched_get_priority_max(SCHED_FIFO);
    
    if (sched_setscheduler(tid, SCHED_FIFO, &param) != 0) {
        std::cerr << "Failed to set SCHED_FIFO for thread " << tid 
                  << " (errno: " << errno << ")" << std::endl;
        success = false;
    }

    // Get and print current core
    int current_core = sched_getcpu();
    std::cout << "Thread " << tid << " requested core " << core_id 
              << ", currently on core " << current_core 
              << " (success: " << success << ")" << std::endl;
              
    // Verify we're on the right core
    if (current_core != core_id) {
        std::cerr << "Thread " << tid << " running on wrong core: " 
                  << current_core << " instead of " << core_id << std::endl;
        success = false;
    }

    return success;
}

} // namespace thread_utils