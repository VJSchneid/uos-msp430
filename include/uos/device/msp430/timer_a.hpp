#pragma once

#include <uos/detail/task_list.hpp>
#include <uos/device/msp430/scheduler.hpp>

namespace uos::dev::msp430 {

template<typename Scheduler>
struct timer_a_base {

    struct task_data {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content

        // starting timepoint is used to check wether an interrupt was missed
        // when configuring the next wakeup_time
        volatile unsigned starting_timepoint;
        volatile unsigned ticks;
    };

    using task_list_t = task_list<task_data, Scheduler>;
    using task_t = task_list_t::task_t;

    static task_t *find_next_ready_task(task_list_t &tl, unsigned current_time) noexcept {
        if (tl.empty()) return nullptr;

        task_t *next_task = nullptr;
        unsigned min_ticks_until_trigger = 0xffff;

        for (auto &task : tl) {
            unsigned ticks_until_trigger = task.starting_timepoint + task.ticks - current_time - 1;
            if (ticks_until_trigger <= min_ticks_until_trigger && current_time - task.starting_timepoint < task.ticks) {
                next_task = &task;
                min_ticks_until_trigger = ticks_until_trigger;
            }
        }
        return next_task;
    }
};

template<typename HWLayer, typename Scheduler>
struct timer_a : timer_a_base<Scheduler> {
    using base = timer_a_base<Scheduler>;

    struct delay_t : private base::task_t {
        delay_t(unsigned ticks_from_now) noexcept : base::task_t(waiting_tasks_.create()) {
            base::task_t::starting_timepoint = HWLayer::current_time();
            base::task_t::ticks = ticks_from_now;
            waiting_tasks_.prepend(*this);
        }

        /// @warning the current delay has to be expired (e.g. by calling sleep)!
        void refresh(unsigned additional_ticks) {
            unsigned old_timepoint = base::task_t::starting_timepoint + 
                                     base::task_t::ticks;
            base::task_t::starting_timepoint = old_timepoint;
            base::task_t::ticks = additional_ticks;
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
        Scheduler::prepare_suspend();

        bool suspend = true;

        // stop timer before updating TAxCCRx
        // as recommended in Family User Guide (section 13.2.4.2)
        HWLayer::stop_timer();
        auto time = HWLayer::current_time();
        // check if sleep timepoint is in future
        if (time - my_task.starting_timepoint < my_task.ticks) {
            // check if currently set wakeup_time has to wait longer as this delay
            if (HWLayer::wakeup_time() - time > my_task.starting_timepoint + my_task.ticks - time) {
                // wakeup time has to be updated
                HWLayer::wakeup_time(my_task.starting_timepoint + my_task.ticks);
            }
        }
        else {
            // delay is already expired, do not suspend
            suspend = false;
        }
        HWLayer::enable_timer();

        
        if (suspend) {
            Scheduler::suspend_me();
        }
    }

private:
    friend HWLayer;

    static inline void __attribute__((always_inline)) handle_isr() noexcept {
        for (auto &task : waiting_tasks_) {
            // unblock all tasks which expired
            if (HWLayer::wakeup_time() == task.starting_timepoint + task.ticks) {
                Scheduler::unblock(task.nr);
            }
        }

        for (auto next_task = base::find_next_ready_task(waiting_tasks_, HWLayer::wakeup_time());
            next_task != nullptr;
            next_task = base::find_next_ready_task(waiting_tasks_, HWLayer::wakeup_time())) {
            
            // stop timer before updating TAxCCRx
            // as recommended in Family User Guide (section 13.2.4.2)
            HWLayer::stop_timer();
            HWLayer::wakeup_time(next_task->starting_timepoint + next_task->ticks);

            // check if delay is not expired
            if (HWLayer::current_time() - next_task->starting_timepoint < next_task->ticks) {
                break; // did not miss interrupt :)
            }

            // delay already expired: set next task and unblock current task manually
            Scheduler::unblock(next_task->nr);
            // TODO: rename unblock to resume
        }
        HWLayer::enable_timer();
    }

    static void check_stop() noexcept {
        if (waiting_tasks_.empty()) {
            HWLayer::stop_timer(); // work is done: stop timer
        }
    }

    static base::task_list_t waiting_tasks_;
};

template<typename HWLayer, typename Scheduler>
typename timer_a_base<Scheduler>::task_list_t timer_a<HWLayer, Scheduler>::waiting_tasks_;

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
        TA0CTL = TASSEL__SMCLK | ID__2 | MC__CONTINUOUS;
    }

    static inline void stop_timer() noexcept {
        TA0CTL = TA0CTL & ~MC_3;
    }

    static inline bool running() noexcept {
        return TA0CTL & MC__CONTINUOUS;
    }

    static void __attribute__((interrupt(TIMER0_A0_VECTOR))) isr();
};

using timer_a0 = timer_a<timer_a0_layer, scheduler>;
#endif

}