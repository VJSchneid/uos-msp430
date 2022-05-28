#pragma once

#include <uos/device/msp430/task_list.hpp>

namespace uos::dev::msp430 {

struct timer_base {

    struct task_data {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content
        volatile unsigned ticks;
        volatile unsigned from_timepoint;
    };
};

struct timer : timer_base {
    using task_t = task_list<task_data>::task_t;

    static void sleep(unsigned ticks) noexcept;

    static bool is_someone_waiting() noexcept;


private:
    static task_t *find_next_ready_task(unsigned current_time) noexcept;

    static void check_start() noexcept;
    static void check_stop() noexcept;

    void __attribute__((interrupt(TIMER0_A0_VECTOR))) isr();

    static task_list<task_data> waiting_tasks_;
};

}