#pragma once

#include <uos/device/msp430/task_list.hpp>

namespace uos::dev::msp430 {

struct timer_a_base {

    struct task_data {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content
        volatile unsigned ticks;
        volatile unsigned from_timepoint;
    };

    using task_t = task_list<task_data>::task_t;

    static task_t *find_next_ready_task(task_list<task_data> &tl, unsigned current_time) noexcept;
};

template<typename HWLayer>
struct timer_a : timer_a_base {
    static void __attribute__ ((noinline)) sleep(unsigned ticks) noexcept {
        if (ticks <= 0) return;

        // prepare suspend (i.e. reset block ctr to zero)
        scheduler::prepare_suspend();

        bool suspend = true;

        auto my_task = waiting_tasks_.create();
        my_task.ticks = ticks; 

        // after setting from_timepoint we have to make sure that
        // the interrupt for short sleeps is not missed
        my_task.from_timepoint = HWLayer::current_time();

        // insert waiting_task to list
        waiting_tasks_.prepend(my_task);

        // check if current sleep should result in next wakeup interrupt
        if (!HWLayer::running() ||
            (HWLayer::wakeup_time() - my_task.from_timepoint) >= ticks) {

            // update wakeup time as this sleep is waked up sooner
            HWLayer::wakeup_time(my_task.from_timepoint + my_task.ticks);

            // check wether the interrupt was (likely) missed
            // i.e. current_time reached wakeup_time before it was set
            auto *task = &my_task;
            while (HWLayer::current_time() - task->from_timepoint >= task->ticks) {
                suspend = false; // we do not need to suspend anymore
                if (HWLayer::wakeup_time() != task->from_timepoint + task->ticks) {
                    // wakeup_time was already updated from isr
                    // no work must be done here
                    break;
                }
                // update for next waiting task
                task = find_next_ready_task(waiting_tasks_, HWLayer::wakeup_time());
                if (!task) {
                    // no task available
                    break;
                }

                // update wakeup_time to cover next task
                HWLayer::wakeup_time(task->from_timepoint + task->ticks);
            }
        }

        // make sure timer is running
        HWLayer::enable_timer();

        if (suspend) {
            scheduler::suspend_me();
        }

        waiting_tasks_.remove(my_task);
        check_stop();
        
    }

private:
    friend HWLayer;

    static void check_stop() noexcept {
        for (auto &task : waiting_tasks_) {
            if (scheduler::is_blocked(task.nr)) {
                return;
            }
        }
        HWLayer::stop_timer(); // work is done: stop timer
    }

    static task_list<task_data> waiting_tasks_;
};

template<typename HWLayer>
task_list<timer_a_base::task_data> timer_a<HWLayer>::waiting_tasks_;

struct timer_a0_layer {
    static inline unsigned wakeup_time() noexcept {
        return TA0CCR0;
    }

    static inline unsigned current_time() noexcept {
        return TA0R;
    }

    static inline void wakeup_time(unsigned timestamp) noexcept {
        TA0CCR0 = timestamp;
    }

    static inline void enable_timer() noexcept {
        TA0CCTL0 = CCIE;
        TA0CTL = TASSEL__SMCLK | ID__8 | MC__CONTINUOUS;
    }

    static inline void stop_timer() noexcept {
        TA0CTL = TA0CTL & ~MC_3;
    }

    static inline bool running() noexcept {
        return TA0CTL & MC__CONTINUOUS;
    }

    static void __attribute__((interrupt(TIMER0_A0_VECTOR))) isr();
};

using timer_a0 = timer_a<timer_a0_layer>;

}