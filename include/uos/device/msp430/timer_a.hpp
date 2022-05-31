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

    struct delay_t : task_t {
        delay_t(unsigned ticks_from_now) noexcept : task_t(waiting_tasks_.create()) {
            ticks = ticks_from_now;
            from_timepoint = HWLayer::current_time();
            waiting_tasks_.prepend(*this);
        }

        ~delay_t() noexcept {
            waiting_tasks_.remove(*this);
            check_stop();
        }
    };

    // TODO: test me!
    static delay_t timestamp_from_now(unsigned ticks) noexcept {
        return delay_t(ticks);
    }

    static void sleep(unsigned ticks) noexcept {
        if (ticks <= 0) return;

        // prepare suspend (i.e. reset block ctr to zero)
        scheduler::prepare_suspend();

        auto my_task = timestamp_from_now(ticks);

        sleep(my_task);
    }

    // TODO: test me!
    static void sleep(timer_a_base::task_t &my_task) noexcept {
        bool suspend = true;

        // since timestamp creation we have to make sure that
        // the interrupt for short sleeps is not missed

        // check if current sleep should result in next wakeup interrupt
        if (!HWLayer::running() ||
            (HWLayer::wakeup_time() - my_task.from_timepoint) >= my_task.ticks) {

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

        if (suspend) {
            // make sure timer is running
            HWLayer::enable_timer();

            scheduler::suspend_me();
        }
    }

private:
    friend HWLayer;

    static inline void __attribute__((always_inline)) handle_isr() noexcept {
        for (auto &task : waiting_tasks_) {
            if (HWLayer::current_time() - task.from_timepoint >= task.ticks) {
                scheduler::unblock(task.nr);
            }
        }

        for (auto next_task = find_next_ready_task(waiting_tasks_, HWLayer::wakeup_time());
            next_task != nullptr;
            next_task = find_next_ready_task(waiting_tasks_, HWLayer::wakeup_time())) {
            
            HWLayer::wakeup_time(next_task->from_timepoint + next_task->ticks);

            if (HWLayer::current_time() - next_task->from_timepoint < next_task->ticks) {
                break; // did not miss interrupt :)
            }

            // we might missed interrupt: set next task and unblock current task manually
            scheduler::unblock(next_task->nr);
            // TODO: rename unblock to resume
        }
    }

    static void check_stop() noexcept {
        if (waiting_tasks_.empty()) {
            HWLayer::stop_timer(); // work is done: stop timer
        }
    }

    static task_list<task_data> waiting_tasks_;
};

template<typename HWLayer>
task_list<timer_a_base::task_data> timer_a<HWLayer>::waiting_tasks_;

#ifdef UOS_DEV_MSP430_ENABLE_TIMERA0
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
        TA0CTL = TASSEL__SMCLK | ID__1 | MC__CONTINUOUS;
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
#endif

}