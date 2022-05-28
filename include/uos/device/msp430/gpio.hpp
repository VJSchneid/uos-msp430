#pragma once

#include <uos/device/msp430/scheduler.hpp>

#include <msp430.h>

namespace uos::dev::msp430 {

struct gpio_base {
    using mask_t = unsigned char;

    struct waiting_task {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content
        volatile unsigned nr;
        volatile mask_t mask;
        waiting_task * volatile next;
    };
};

template<typename PortLayer>
struct gpio : gpio_base {
    static void wait_for_change(mask_t mask) {
        if (!mask) return;
        scheduler::prepare_block();

        waiting_task my_task;
        my_task.nr = scheduler::taskid();
        my_task.mask = mask;

        my_task.next = waiting_tasks_;
        waiting_tasks_ = &my_task;

        // set interrupt enable with bitmask
        PortLayer::enable_interrupt(mask);

        remove_from_list(my_task);
    }

    static waiting_task * volatile waiting_tasks_;

protected:
    friend PortLayer;

    static void remove_from_list(waiting_task &t) {
        waiting_task *task;
        waiting_task *volatile *prev_task_next = &waiting_tasks_;
        for (task = waiting_tasks_; task != nullptr; task = task->next) {
            if (task == &t) {
                *prev_task_next = t.next;
                return;
            }
            prev_task_next = &task->next;
        }
    }
};

template<typename PortLayer>
gpio<PortLayer>::waiting_task * volatile gpio<PortLayer>::waiting_tasks_ = nullptr;

#ifdef UOS_DEV_MSP430_ENABLE_PORT1
struct port1_layer {
    static inline void enable_interrupt(gpio_base::mask_t mask) {
        P1IE = P1IE | mask;
    }

private:
    static void __attribute__((interrupt(PORT1_VECTOR))) isr();
};

using port1 = gpio<port1_layer>;
#endif

#ifdef UOS_DEV_MSP430_ENABLE_PORT2
struct port2_layer {
    static inline void enable_interrupt(gpio_base::mask_t mask) {
        P2IE = P2IE | mask;
    }

private:
    static void __attribute__((interrupt(PORT2_VECTOR))) isr();
};

using port2 = gpio<port2_layer>;
#endif

}