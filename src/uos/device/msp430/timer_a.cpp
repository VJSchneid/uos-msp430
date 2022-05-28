#include <uos/device/msp430/timer_a.hpp>

namespace uos::dev::msp430 {

task_list<timer_base::task_data> timer::waiting_tasks_;

void timer::sleep(unsigned ticks) noexcept {
    if (ticks <= 0) return;

    // prepare suspend (i.e. reset block ctr to zero)
    scheduler::prepare_suspend();

    bool suspend = true;

    auto my_task = waiting_tasks_.create();
    my_task.ticks = ticks; 

    // after setting from_timepoint we have to make sure that
    // the interrupt for short sleeps is not missed
    my_task.from_timepoint = TA0R;

    // insert waiting_task to list
    waiting_tasks_.prepend(my_task);

    // check if current sleep results in next wakeup
    if (!(TA0CTL & MC__CONTINUOUS) || (TA0CCR0 - my_task.from_timepoint) >= ticks) {
        // set TA0CCR0 as this sleep is waked up sooner
        TA0CCR0 = my_task.from_timepoint + my_task.ticks;


        // check wether the interrupt was (likely) missed
        // i.e. TA0R reached timepoint before TA0CCR0 was set 
        auto *task = &my_task;
        while (TA0R - task->from_timepoint >= task->ticks) {
            suspend = false; // we do not need to suspend anymore
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

    if (suspend) {
        scheduler::suspend_me();
    }

    waiting_tasks_.remove(my_task);
    check_stop();
}

bool timer::is_someone_waiting() noexcept {
    for (auto task = waiting_tasks_.waiting_tasks_; task != nullptr; task = task->next) {
        if (scheduler::is_blocked(task->nr)) {
            return true;
        }
    }
    return false;
}

timer::task_t *timer::find_next_ready_task(unsigned current_time) noexcept {
    if (waiting_tasks_.empty()) return nullptr;

    task_t *next_task = nullptr;
    unsigned next_task_timepoint; // = next_task->from_timepoint - current_time + next_task->ticks;

    for (auto task = waiting_tasks_.waiting_tasks_; task != nullptr; task = task->next) {
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

void timer::check_stop() noexcept {
    if (!is_someone_waiting()) {
        TA0CTL = TA0CTL & ~MC_3; // work is done: stop timer
    }
}

void __attribute__((interrupt(TIMER0_A0_VECTOR))) timer::isr() {
    for (auto task = waiting_tasks_.waiting_tasks_; task != nullptr; task = task->next) {
        if (TA0R - task->from_timepoint >= task->ticks) {
            scheduler::unblock(task->nr);
        }
    }

    for (auto next_task = find_next_ready_task(TA0CCR0);
        next_task != nullptr;
        next_task = find_next_ready_task(TA0CCR0)) {
        
        TA0CCR0 = next_task->from_timepoint + next_task->ticks;

        if (TA0R - next_task->from_timepoint < next_task->ticks) {
            break; // did not miss interrupt :)
        }

        // we might missed interrupt: try next TA0CCR0 and unblock manually
        scheduler::unblock(next_task->nr);
    }

    // wakeup scheduler
    __bic_SR_register_on_exit(LPM0_bits);
}

} // namespace uos
