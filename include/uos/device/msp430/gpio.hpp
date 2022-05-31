#pragma once

#include <uos/device/msp430/task_list.hpp>
#include <uos/device/msp430/scheduler.hpp>

#include <msp430.h>

namespace uos::dev::msp430 {

struct gpio_base {
    using mask_t = unsigned char;

    struct task_data {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content
        volatile mask_t mask;
    };
};

template<typename PortLayer>
struct gpio : gpio_base {
    static void wait_for_change(mask_t mask) {
        if (!mask) return;
        scheduler::prepare_suspend();

        auto my_task = waiting_tasks_.create();
        my_task.mask = mask;
        waiting_tasks_.prepend(my_task);

        // set interrupt enable with bitmask
        PortLayer::enable_interrupt(mask);

        scheduler::suspend_me();

        waiting_tasks_.remove(my_task);
    }

protected:
    friend PortLayer;

    static task_list<task_data> waiting_tasks_;
};

template<typename PortLayer>
task_list<gpio_base::task_data> gpio<PortLayer>::waiting_tasks_;

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