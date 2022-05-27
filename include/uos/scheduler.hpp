#pragma once

#include <uos/thread.hpp>

#include <msp430.h>
#include <uos/device/msp430/scheduler.hpp>
#include <cstdint>

namespace uos {

template <unsigned NrTasks, typename MCULayer> struct scheduler_t : MCULayer {
    struct task_t {
        int blocked = 0;
        void* sp = nullptr;
    };


    void unblock(unsigned tasknr) noexcept {
        // IMPORTANT: This has to be atomic
        if (tasks_[tasknr].blocked >= 0) {
            tasks_[tasknr].blocked = tasks_[tasknr].blocked - 1;
        }
    }

    void prepare_block() noexcept { tasks_[active_task_].blocked = 0; }

    unsigned taskid() noexcept { return active_task_; }

    void init() {
        // first task is suspended without having a registered stack pointer
        tasks_[0].sp = this; // assign just a valid address
    }

    bool is_blocked(unsigned tasknr) { return tasks_[tasknr].blocked > 0; }

    void suspend_me() noexcept {
        int my_tasknr = active_task_;
        tasks_[my_tasknr].blocked = tasks_[my_tasknr].blocked + 1;
        while (true) {
            for (int i = 0; i < NrTasks; i++) {
                if (tasks_[i].sp != nullptr && tasks_[i].blocked <= 0) {
                    if (i != active_task_) {
                        active_task_ = i;
                        // TODO this should be protected? (interrupt disable?)
                        MCULayer::thread_switch(&tasks_[i].sp, &tasks_[my_tasknr].sp);
                    }
                    return;
                }
            }
            MCULayer::sleep_until_interrupt();
        }
    }

    bool add_task(unsigned nr, void *sp, void (*initial_function)()) noexcept {
        if (nr == 0 || nr >= NrTasks || tasks_[nr].sp != nullptr) return false;
        MCULayer::thread_init(&sp, initial_function);
        tasks_[nr].sp = sp;
        return true;
    }

    volatile task_t tasks_[NrTasks];
    volatile unsigned active_task_ = 0;
};

extern scheduler_t<3, dev::msp430::scheduler> scheduler;

} // namespace uos
