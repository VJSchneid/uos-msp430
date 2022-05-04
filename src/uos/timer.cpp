#include <uos/timer.hpp>

#include <msp430.h>

namespace uos {

timer_t timer;

/*void timer_t::sleep(unsigned ticks) noexcept {
    if (ticks == 0) return;

    waiting_task my_task;
    my_task.nr = scheduler.taskid();

    // stop/init timer
    TA0CTL = TASSEL__SMCLK | ID__8;
    TA0CCTL0 = CCIE;

    my_task.ticks = TA0R + ticks;
    if (!is_someone_waiting() || (TA0CCR0 - TA0R) > my_task.ticks - TA0R) {
        TA0CCR0 = my_task.ticks;
    }

    scheduler.prepare_block();

    atomic_insert(my_task);

    TA0CTL = TA0CTL | MC__CONTINUOUS; // start timer

    scheduler.suspend_me();

    atomic_remove(my_task);
    check_stop();
}*/

void timer_t::sleep(unsigned ticks) noexcept {
    if (ticks <= 0) return;

    // prepare block (i.e. reset block ctr to zero)
    scheduler.prepare_block();

    bool block = true;

    waiting_task my_task;
    my_task.nr = scheduler.taskid();
    my_task.ticks = ticks; 

    // after setting from_timepoint we have to make sure that
    // the interrupt for short sleeps is not missed
    my_task.from_timepoint = TA0R;

    // insert waiting_task to list
    my_task.next = waiting_tasks_;
    waiting_tasks_ = &my_task;

    // check if current sleep is next wakeup
    if (!(TA0CTL & MC__CONTINUOUS) || (TA0CCR0 - my_task.from_timepoint) >= ticks) {
        // set TA0CCR0 as this sleep is waked up sooner
        TA0CCR0 = my_task.from_timepoint + my_task.ticks;


        // check wether the interrupt was (likely) missed
        // i.e. TA0R reached timepoint before TA0CCR0 was set 
        waiting_task *task = &my_task;
        while (TA0R - task->from_timepoint >= task->ticks) {
            block = false; // we do not need to suspend anymore
            if (TA0CCR0 != task->from_timepoint + task->ticks) {
                // TA0CCR0 was changed => interrupt was seen
                break;
            }
            // update for next waiting task
            task = find_next_ready_task(TA0CCR0);
            if (!task) {
                // no task available
                // TODO stop timer!
                break;
            }

            // update TA0CCR0 to cover next task
            TA0CCR0 = task->from_timepoint + task->ticks;
        }
    }

    // make sure timer is running
    TA0CCTL0 = CCIE;
    TA0CTL = TASSEL__SMCLK | ID__8 | MC__CONTINUOUS;

    if (block) {
        scheduler.suspend_me();
    }

    atomic_remove(my_task);
    check_stop();
}

bool timer_t::is_someone_waiting() noexcept {
    for (auto task = waiting_tasks_; task != nullptr; task = task->next) {
        if (scheduler.is_blocked(task->nr)) {
            return true;
        }
    }
    return false;
}

timer_t::waiting_task *timer_t::find_next_ready_task(unsigned current_time) noexcept {
    if (waiting_tasks_ == nullptr) return nullptr;

    waiting_task *next_task = nullptr;
    unsigned next_task_timepoint; // = next_task->from_timepoint - current_time + next_task->ticks;

    for (auto task = waiting_tasks_; task != nullptr; task = task->next) {
        if (current_time - task->from_timepoint < task->ticks ) { // task is in sleep
            unsigned this_task_timepoint = task->from_timepoint - current_time + task->ticks;
            if (next_task == nullptr || this_task_timepoint < next_task_timepoint) {
                next_task_timepoint = this_task_timepoint;
                next_task = task;
            }
        }
    }
    return next_task;
}

void timer_t::atomic_insert(waiting_task &t) noexcept {

    /*waiting_task *task;
    waiting_task *volatile *prev_task_next = &waiting_tasks_;
    for (task = timer.waiting_tasks_; task != nullptr; task = task->next) {
        if (t.ticks < task->ticks) {
            break;
        }
        prev_task_next = &task->next;
    }*/
    t.next = waiting_tasks_;
    waiting_tasks_ = &t;
}

void timer_t::atomic_remove(waiting_task &t) noexcept {
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

void timer_t::check_stop() noexcept {
    if (!is_someone_waiting()) {
        TA0CTL = TA0CTL & ~MC_3; // work is done: stop timer
    }
}

/*void __attribute__((interrupt(TIMER0_A0_VECTOR))) timer_a0_isr() {
    timer_t::waiting_task *task;
    timer_t::waiting_task *next_ready_task = nullptr;
    unsigned next_ready_delay;

    TA0CTL = TA0CTL & ~MC_3; // stop timer

    for (task = timer.waiting_tasks_; task != nullptr; task = task->next) {
        if (TA0R - task->from_timepoint >= task->ticks) {
            scheduler.unblock(task->nr);
        } else if (next_ready_task == nullptr ||
                   next_ready_delay < (next_ready_task->ticks - TA0R)) {
            next_ready_delay = task->ticks - TA0R;
            next_ready_task = task;
        }
    }

    if (next_ready_task) {
        // we might miss a wakeup here!
        TA0CCR0 = next_ready_task->ticks;
    }

    TA0CCTL0 = TA0CCTL0 & ~CCIFG;

    TA0CTL = TA0CTL | MC__CONTINUOUS; // start timer
    //__bic_SR_register_on_exit(LPM4_bits);
}*/

void __attribute__((interrupt(TIMER0_A0_VECTOR))) timer_a0_isr() {
    for (auto task = timer.waiting_tasks_; task != nullptr; task = task->next) {
        if (TA0R - task->from_timepoint >= task->ticks) {
            scheduler.unblock(task->nr);
        }
    }

    for (auto next_task = timer.find_next_ready_task(TA0CCR0);
        next_task != nullptr;
        next_task = timer.find_next_ready_task(TA0CCR0)) {
        
        TA0CCR0 = next_task->from_timepoint + next_task->ticks;

        if (TA0R - next_task->from_timepoint < next_task->ticks) {
            break; // did not miss interrupt :)
        }

        // we might missed interrupt: try next TA0CCR0 and unblock manually
        scheduler.unblock(next_task->nr);
        //TA0CCTL0 = TA0CCTL0 & ~CCIFG;
    }

    // no need to clear ifg
    //TA0CCTL0 = TA0CCTL0 & ~CCIFG;

    //TA0CTL = TA0CTL | MC__CONTINUOUS; // start timer

    // wakeup scheduler
    __bic_SR_register_on_exit(LPM0_bits);
}

} // namespace uos
