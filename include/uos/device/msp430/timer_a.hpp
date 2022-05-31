#pragma once

#include <uos/device/msp430/task_list.hpp>

namespace uos::dev::msp430 {

struct timer_a_base {

    struct task_data {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content

        // starting timepoint is used to check wether an interrupt was missed
        // when configuring the next wakeup_time
        volatile unsigned starting_timepoint;
        volatile unsigned ticks;
    };

    using task_t = task_list<task_data>::task_t;

    static task_t *find_next_ready_task(task_list<task_data> &tl, unsigned current_time) noexcept;
};

template<typename HWLayer>
struct timer_a : timer_a_base {

    struct delay_t : private task_t {
        delay_t(unsigned ticks_from_now) noexcept : task_t(waiting_tasks_.create()) {
            starting_timepoint = HWLayer::current_time();
            ticks = ticks_from_now;
            waiting_tasks_.prepend(*this);
        }

        /// @warning the current delay has to be expired (e.g. by calling sleep)!
        void refresh(unsigned additional_ticks) {
            unsigned old_timepoint = starting_timepoint + ticks;
            starting_timepoint = old_timepoint;
            ticks = additional_ticks;
        }

        ~delay_t() noexcept {
            waiting_tasks_.remove(*this);
            check_stop();
        }
        friend timer_a;
    };

    static delay_t delay_from_now(unsigned ticks) noexcept {
        return delay_t(ticks);
    }

    static void sleep(unsigned ticks) noexcept {
        if (ticks <= 0) return;

        auto my_task = delay_from_now(ticks);

        sleep(my_task);
    }

    // TODO: test me!
    static void sleep(delay_t &my_task) noexcept {
        // prepare suspend (i.e. reset block ctr to zero)
        scheduler::prepare_suspend();

        bool suspend = true;

        // since timestamp creation we have to make sure that
        // the interrupt for short sleeps is not missed

        // check if current sleep should result in next wakeup interrupt
        if (!HWLayer::running() ||
            (HWLayer::wakeup_time() - my_task.starting_timepoint) >= my_task.ticks) {

            // update wakeup time as this sleep is waked up sooner
            HWLayer::wakeup_time(my_task.starting_timepoint + my_task.ticks);

            // check wether the interrupt was (likely) missed
            // i.e. current_time reached wakeup_time before it was set
            task_t *task = &my_task;
            while (HWLayer::current_time() - task->starting_timepoint >= task->ticks) {
                suspend = false; // we do not need to suspend anymore
                if (HWLayer::wakeup_time() != task->starting_timepoint + task->ticks) {
                    // wakeup_time was already updated from ISR
                    // no further work has to be done here
                    break;
                }
                // update for next waiting task
                task = find_next_ready_task(waiting_tasks_, HWLayer::wakeup_time());
                if (!task) {
                    // no task available
                    break;
                }

                // update wakeup_time to cover next task
                HWLayer::wakeup_time(task->starting_timepoint + task->ticks);
            }
        }

        if (suspend) {
            // make sure the timer is running
            HWLayer::enable_timer();

            scheduler::suspend_me();
        }
    }

private:
    friend HWLayer;

    static inline void __attribute__((always_inline)) handle_isr() noexcept {
        for (auto &task : waiting_tasks_) {
            // unblock all tasks which expired
            if (HWLayer::wakeup_time() == task.starting_timepoint + task.ticks) {
                scheduler::unblock(task.nr);
            }
        }

        for (auto next_task = find_next_ready_task(waiting_tasks_, HWLayer::wakeup_time());
            next_task != nullptr;
            next_task = find_next_ready_task(waiting_tasks_, HWLayer::wakeup_time())) {
            
            HWLayer::wakeup_time(next_task->starting_timepoint + next_task->ticks);

            // check if delay already expired
            if (HWLayer::current_time() - next_task->starting_timepoint < next_task->ticks) {
                break; // did not miss interrupt :)
            }

            // we missed interrupt: set next task and unblock current task manually
            scheduler::unblock(next_task->nr); // TODO can this result in multiple unblocks?
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