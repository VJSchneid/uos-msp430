#pragma once

#include <uos/detail/task_list.hpp>
#include <uos/device/msp430/scheduler.hpp>

namespace uos::dev::msp430 {

template<typename Scheduler>
struct gpio_base {
    using mask_t = unsigned char;

    struct task_data {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content
        volatile mask_t mask;
    };

    using task_list_t = task_list<task_data, Scheduler>;
};

template<typename PortLayer, typename Scheduler>
struct gpio : gpio_base<Scheduler> {
    using base = gpio_base<Scheduler>;

    static void wait_for_change(base::mask_t mask) {
        if (!mask) return;
        Scheduler::prepare_suspend();

        auto my_task = waiting_tasks_.create();
        my_task.mask = mask;
        waiting_tasks_.prepend(my_task);

        // set interrupt enable with bitmask
        PortLayer::enable_interrupt(mask);

        Scheduler::suspend_me();

        waiting_tasks_.remove(my_task);
    }

protected:
    friend PortLayer;

    static base::task_list_t waiting_tasks_;
};

template<typename PortLayer, typename Scheduler>
typename gpio_base<Scheduler>::task_list_t gpio<PortLayer, Scheduler>::waiting_tasks_;

#ifdef UOS_DEV_MSP430_ENABLE_PORT1
/// @note uses uos::dev::msp430::scheduler
struct port1_layer {
    static inline void enable_interrupt(unsigned char mask) {
        P1IE = P1IE | mask;
    }

private:
    static void __attribute__((interrupt(PORT1_VECTOR))) isr();
};

using port1 = gpio<port1_layer, scheduler>;
#endif

#ifdef UOS_DEV_MSP430_ENABLE_PORT2

/// @note uses uos::dev::msp430::scheduler
struct port2_layer {
    static inline void enable_interrupt(unsigned char mask) {
        P2IE = P2IE | mask;
    }

private:
    static void __attribute__((interrupt(PORT2_VECTOR))) isr();
};

using port2 = gpio<port2_layer, scheduler>;
#endif

}