#pragma once

#include <uos/device/msp430/task_list.hpp>

namespace uos::dev::msp430 {

template<typename Scheduler>
struct usci_base {

    struct task_data {
        const char * volatile data;
        volatile unsigned length;
    };

    using task_list_t = task_list<task_data, Scheduler>;
    using task_t = task_list_t::task_t;

    static inline task_t *get_active_task(task_list_t &tl) {
        task_t *last_task_with_data = nullptr;
        for (auto &task : tl) {
            if (task.length > 0) {
                last_task_with_data = &task;
            } else {
                // list is sorted chronological
                // therefore we found the end and can break here
                break;
            }
        }
        return last_task_with_data;
    }
};

template<typename HWLayer, typename Scheduler>
struct eusci : usci_base<Scheduler> {
    using base = usci_base<Scheduler>;

    static void transmit(const char *data, unsigned length) {
        // TODO power save and maybe reset with UCSWRST?
        if (length == 0) return;
        Scheduler::prepare_suspend();

        auto my_task = waiting_tx_tasks_.create();
        my_task.data = data;
        my_task.length = length;

        waiting_tx_tasks_.prepend(my_task);

        HWLayer::disable_tx_interrupt();
        if (not HWLayer::tx_busy() && not HWLayer::tx_interrupt_pending()) {
            HWLayer::raise_tx_interrupt(); // let the ISR do the dirty work
        }
        HWLayer::enable_tx_interrupt();

        Scheduler::suspend_me();

        waiting_tx_tasks_.remove(my_task);
    }

    //void transmit(const char *str);

    template<unsigned length>
    static void transmit(const char(&str)[length]) {
        // add compile time string
        transmit(str, length-1);
    }

private:
    friend HWLayer;
    static base::task_list_t waiting_tx_tasks_;
};

template<typename HWLayer, typename Scheduler>
typename usci_base<Scheduler>::task_list_t 
    eusci<HWLayer, Scheduler>::waiting_tx_tasks_;

#ifdef UOS_DEV_MSP430_ENABLE_EUSCIA1
struct eusci_a1_layer {
    static void enable_tx_interrupt() noexcept {
        UCA1IE = UCA1IE | UCTXCPTIFG;
    }

    static void disable_tx_interrupt() noexcept {
        UCA1IE = UCA1IE & ~UCTXCPTIFG;
    }

    static bool tx_busy() noexcept {
        return UCA1STATW & UCBUSY;
    }

    static bool tx_interrupt_pending() noexcept {
        return UCA1IFG & UCTXCPTIFG;
    }

    static void raise_tx_interrupt() noexcept {
        UCA1IFG = UCA1IFG | UCTXCPTIFG;
    }

    static void __attribute__((interrupt(USCI_A1_VECTOR))) isr();
};

using eusci_a1 = eusci<eusci_a1_layer, scheduler>;
#endif

#ifdef UOS_DEV_MSP430_ENABLE_EUSCIB0
struct eusci_b0_layer {
    static void enable_tx_interrupt() noexcept {
        UCB0IE = UCB0IE | UCTXCPTIFG;
    }

    static void disable_tx_interrupt() noexcept {
        UCB0IE = UCB0IE & ~UCTXCPTIFG;
    }

    static bool tx_busy() noexcept {
        return UCB0STATW & UCBUSY;
    }

    static bool tx_interrupt_pending() noexcept {
        return UCB0IFG & UCTXCPTIFG;
    }

    static void raise_tx_interrupt() noexcept {
        UCB0IFG = UCB0IFG | UCTXCPTIFG;
    }

    static void __attribute__((interrupt(USCI_B0_VECTOR))) isr();
};

using eusci_b0 = eusci<eusci_b0_layer, scheduler>;
#endif

}