#pragma once

#ifndef UOS_NUMBER_OF_TASKS
#define UOS_NUMBER_OF_TASKS 4
#endif

namespace uos {

template <typename MCULayer>
struct basic_scheduler {

    struct task_t {
        int blocked = 0;
        void* sp = nullptr;
    };

    static void unblock(unsigned tasknr) noexcept {
        // IMPORTANT: This has to be atomic
        if (tasks_[tasknr].blocked >= 0) {
            tasks_[tasknr].blocked = tasks_[tasknr].blocked - 1;
        }
    }

    static void prepare_suspend() noexcept { tasks_[active_task_].blocked = 0; }

    static unsigned taskid() noexcept { return active_task_; }

    static void init() {
        // first task is suspended without having a registered stack pointer
        tasks_[0].sp = const_cast<unsigned *>(&active_task_); // assign just a valid address
    }

    static bool is_blocked(unsigned tasknr) { return tasks_[tasknr].blocked > 0; }

    static void suspend_me() noexcept {
        int my_tasknr = active_task_;
        tasks_[my_tasknr].blocked = tasks_[my_tasknr].blocked + 1;
        while (true) {
            for (int i = 0; i < UOS_NUMBER_OF_TASKS; i++) {
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

    static bool add_task(unsigned nr, void *sp, void (*initial_function)()) noexcept {
        if (nr == 0 || nr >= UOS_NUMBER_OF_TASKS || tasks_[nr].sp != nullptr) return false;
        MCULayer::thread_init(&sp, initial_function);
        tasks_[nr].sp = sp;
        return true;
    }

    static volatile task_t tasks_[UOS_NUMBER_OF_TASKS];
    static volatile unsigned active_task_;
};

template<typename MCULayer>
volatile unsigned basic_scheduler<MCULayer>::active_task_ = 0;

template<typename MCULayer>
volatile basic_scheduler<MCULayer>::task_t basic_scheduler<MCULayer>::tasks_[UOS_NUMBER_OF_TASKS];

} // namespace uos
