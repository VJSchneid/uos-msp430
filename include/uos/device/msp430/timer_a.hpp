#pragma once

#include <uos/detail/task_list.hpp>
#include <uos/device/msp430/scheduler.hpp>
#include <stdint.h>

namespace uos::dev::msp430 {

template<typename Scheduler>
struct timer_a_base {

    struct task_data {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content

        volatile uint16_t wakeup_time;

        inline bool is_expired_in(uint16_t old_time, uint16_t new_time) noexcept {
            //if (old_time == new_time) return true; // one complete clock turn
            uint16_t time_step = new_time - old_time;
            uint16_t time_after_trigger = new_time - wakeup_time;
            return time_step > time_after_trigger;
        }
    };

    using task_list_t = task_list<task_data, Scheduler>;
    using task_t = task_list_t::task_t;

#if false
    static task_t *find_next_ready_task(task_list_t &tl, uint16_t current_time) noexcept {
        if (tl.empty()) return nullptr;

        task_t *next_task = nullptr;
        uint16_t min_ticks_until_trigger = 0xffff;

        for (auto &task : tl) {
            if (task.is_expired(current_time)) continue;

            uint16_t ticks_until_trigger = task.starting_timepoint + task.ticks - current_time;
            if (ticks_until_trigger <= min_ticks_until_trigger) {
                next_task = &task;
                min_ticks_until_trigger = ticks_until_trigger;
            }
        }
        return next_task;
    }
#endif
};

template<typename HWLayer, typename Scheduler>
struct timer_a : timer_a_base<Scheduler> {
    using base = timer_a_base<Scheduler>;

    struct delay_t {
        uint16_t reference_timepoint;
        uint16_t ticks;
    };

#if false
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
#endif

    static void init() noexcept {
        last_taxr_ = HWLayer::current_time();
    }

    /**
     * suspends the current execution until the time
     * specified in `delay` was reached
     * 
     * @warning no more than 0xffff ticks have to be elapsed
     *          since creation of delay or its last sleep
     * 
     * @note    maximal delay is 0xffff
     */
    static void sleep(delay_t delay) noexcept {
        // TODO
    }

    /**
     * suspends the current execution for at least `ticks` ticks
     */
    static void sleep(uint16_t ticks) noexcept {
        if (ticks <= 0) return;

        uint16_t reference_time = HWLayer::current_time();

        auto task = waiting_tasks_.create();
        task.wakeup_time = reference_time + ticks;


        sleep_since_ref(task, reference_time);
        waiting_tasks_.remove(task);
    }

    static bool idle() noexcept {
        return waiting_tasks_.empty();
    }

private:
    friend HWLayer;

    // TODO: test me!
    static void sleep_since_ref(base::task_t &task, uint16_t reference_time) noexcept {
        Scheduler::prepare_suspend();

        // stop timer before updating TAxCCRx
        // as recommended in Family User Guide (section 13.2.4.2)
        HWLayer::stop_timer();

        uint16_t time = HWLayer::current_time();

        // when stopping the timer an interrupt might be missed
        // so it is necessary to check if any old task expired
        uint16_t min_ticks_until_trigger = 0xffff;
        uint16_t next_wakeup_time = time-1;
        for (auto &old_task : waiting_tasks_) {
            // TODO: last_taxr -> time might be longer than 0xffff cycles (MAYFAIL)
            if (old_task.is_expired_in(last_taxr_, time)) {
                Scheduler::unblock(old_task.nr);
            } else {
                uint16_t ticks_until_trigger = old_task.wakeup_time - time;
                if (min_ticks_until_trigger > ticks_until_trigger) {
                    next_wakeup_time = old_task.wakeup_time;
                    min_ticks_until_trigger = ticks_until_trigger;
                }
            }
        }

        waiting_tasks_.prepend(task);

        bool suspend = true;
        // check if task is not expired already
        if (!task.is_expired_in(reference_time, time)) {
            
            uint16_t ticks_until_trigger = task.wakeup_time - time;
            if (min_ticks_until_trigger > ticks_until_trigger) {
                next_wakeup_time = task.wakeup_time;
                min_ticks_until_trigger = ticks_until_trigger;
            }
        } else {
            suspend = false;
        }

        last_taxr_ = time;
        HWLayer::wakeup_time(next_wakeup_time);

        HWLayer::enable_timer();

        if (suspend) {
            Scheduler::suspend_me();
        }

        waiting_tasks_.remove(task);

        maybe_stop_timer();

#if false
        // prepare suspend (i.e. reset block ctr to zero)
        Scheduler::prepare_suspend();

        bool suspend = true;

        // stop timer before updating TAxCCRx
        // as recommended in Family User Guide (section 13.2.4.2)
        HWLayer::stop_timer();
        auto time = HWLayer::current_time();

        // check that sleep timepoint is in future
        if (!my_task.is_expired(time)) {
            // check if currently set wakeup_time has to wait longer as this delay
            uint16_t current_time_to_wait = HWLayer::wakeup_time() - time;
            uint16_t my_task_time_to_wait = my_task.wakeup_time() - time;
            if (waiting_tasks_.waiting_tasks_->next == nullptr ||
                current_time_to_wait > my_task_time_to_wait) {
                // wakeup time has to be updated
                HWLayer::wakeup_time(my_task.wakeup_time());
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
#endif
    }

    static inline void __attribute__((always_inline)) handle_isr() noexcept {
        HWLayer::stop_timer();

        uint16_t time = HWLayer::current_time();

        uint16_t min_ticks_until_trigger = 0xffff;
        uint16_t next_wakeup_time = time-1;
        for (auto &old_task : waiting_tasks_) {
            // TODO: last_taxr -> time might be longer than 0xffff cycles (MAYFAIL)
            if (old_task.is_expired_in(last_taxr_, time)) { // TODO: maybe we should check two ranges: last_taxr_ -> wakeup_time and wakeup_time -> time
                Scheduler::unblock(old_task.nr);
            } else {
                uint16_t ticks_until_trigger = old_task.wakeup_time - time;
                if (min_ticks_until_trigger > ticks_until_trigger) {
                    next_wakeup_time = old_task.wakeup_time;
                    min_ticks_until_trigger = ticks_until_trigger;
                }
            }
        }

        last_taxr_ = time;
        HWLayer::wakeup_time(next_wakeup_time);

        HWLayer::enable_timer();

#if false
        for (auto &task : waiting_tasks_) {
            // unblock all tasks which expired
            if (HWLayer::wakeup_time() == task.wakeup_time()) {
                Scheduler::unblock(task.nr);
            }
        }

        // stop timer before updating TAxCCRx
        // as recommended in Family User Guide (section 13.2.4.2)
        HWLayer::stop_timer();
        auto time = HWLayer::current_time();
        for (auto next_task = base::find_next_ready_task(waiting_tasks_, HWLayer::wakeup_time());
            next_task != nullptr;
            next_task = base::find_next_ready_task(waiting_tasks_, next_task->wakeup_time())) { // TODO miss multiple overunned wakeups 

            // check if delay is not expired
            if (!next_task->is_expired(time)) {
                HWLayer::wakeup_time(next_task->wakeup_time());
                break; // did not miss interrupt :)
            }

            // delay already expired: set next task and unblock current task manually
            Scheduler::unblock(next_task->nr);
            // TODO: rename unblock to resume
        }
        HWLayer::enable_timer();
#endif
    }

    static void maybe_stop_timer() noexcept {
        if (waiting_tasks_.empty()) {
            HWLayer::stop_timer();
        }
    }

    static volatile uint16_t last_taxr_;
    static base::task_list_t waiting_tasks_;
};

template<typename HWLayer, typename Scheduler>
typename timer_a_base<Scheduler>::task_list_t timer_a<HWLayer, Scheduler>::waiting_tasks_;

template<typename HWLayer, typename Scheduler>
volatile uint16_t timer_a<HWLayer, Scheduler>::last_taxr_ = 0;

}