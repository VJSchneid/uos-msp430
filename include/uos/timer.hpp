#pragma once

#include <uos/scheduler.hpp>

#include <msp430.h>

namespace uos {

struct timer_t {

    struct waiting_task {
        unsigned nr;
        unsigned ticks;
        unsigned from_timepoint;
        waiting_task *volatile next = nullptr;
    };

    void sleep(unsigned ticks) noexcept;

    bool is_someone_waiting() noexcept;

    waiting_task *volatile waiting_tasks_ = nullptr;
    unsigned epoch_ = 0;

    waiting_task *find_next_ready_task(unsigned current_time) noexcept;
private:

    void atomic_insert(waiting_task &t) noexcept;
    void atomic_remove(waiting_task &t) noexcept;

    void check_start() noexcept;
    void check_stop() noexcept;
};

extern timer_t timer;

void timer_a0_isr();

} // namespace uos
