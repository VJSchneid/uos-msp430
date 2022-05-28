#include <uos/device/msp430/timer_a.hpp>

namespace uos::dev::msp430 {

timer_a_base::task_t *timer_a_base::find_next_ready_task(task_list<task_data> &tl, unsigned current_time) noexcept {
    if (tl.empty()) return nullptr;

    task_t *next_task = nullptr;
    unsigned next_task_timepoint; // = next_task->from_timepoint - current_time + next_task->ticks;

    for (auto task = tl.waiting_tasks_; task != nullptr; task = task->next) {
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

void __attribute__((interrupt(TIMER0_A0_VECTOR))) timer_a0_layer::isr() {
    for (auto task = timer_a0::waiting_tasks_.waiting_tasks_; task != nullptr; task = task->next) {
        if (TA0R - task->from_timepoint >= task->ticks) {
            scheduler::unblock(task->nr);
        }
    }

    for (auto next_task = timer_a0::find_next_ready_task(timer_a0::waiting_tasks_, TA0CCR0);
        next_task != nullptr;
        next_task = timer_a0::find_next_ready_task(timer_a0::waiting_tasks_, TA0CCR0)) {
        
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
