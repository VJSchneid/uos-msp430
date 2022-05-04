#pragma once

#include <uos/thread.hpp>

#include <msp430.h>
#include <cstdint>

namespace uos {

struct task_t {
    int blocked = 0;
    void* sp = nullptr;
};

template <unsigned NrTasks> struct scheduler_t {

    void unblock(unsigned tasknr) noexcept {
        // IMPORTANT: This has to be atomic
        if (tasks_[tasknr].blocked >= 0) {
            tasks_[tasknr].blocked = tasks_[tasknr].blocked - 1;
        }
    }

    void prepare_block() noexcept { tasks_[active_task_].blocked = 0; }

    unsigned taskid() noexcept { return active_task_; }

    bool is_blocked(unsigned tasknr) { return tasks_[tasknr].blocked > 0; }

    void suspend_me() noexcept {
        int my_tasknr = active_task_;
        tasks_[my_tasknr].blocked = 1;
        while (true) {
            for (int i = 0; i < NrTasks; i++) {
                if (tasks_[i].sp != nullptr && tasks_[i].blocked <= 0) {
                    if (i != active_task_) {
                        active_task_ = i;
                        // TODO this should be protected? (interrupt disable?)
                        thread_switch(&tasks_[i].sp, &tasks_[my_tasknr].sp);
                    }
                    return;
                }
            }
            __bis_SR_register(LPM0_bits);
        }
    }

    bool add_task(unsigned nr, void *sp, void (*initial_function)()) noexcept {
        if (nr == 0 || nr >= NrTasks || tasks_[nr].sp != nullptr) return false;
        thread_init(&sp, initial_function);
        tasks_[nr].sp = sp;
        return true;
    }

    volatile task_t tasks_[NrTasks];
    volatile unsigned active_task_ = 0;
};

extern scheduler_t<3> scheduler;

} // namespace uos
