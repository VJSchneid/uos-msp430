#include <uos/device/msp430/timer_a.hpp>

namespace uos::dev::msp430 {

timer_a_base::task_t *timer_a_base::find_next_ready_task(task_list<task_data> &tl, unsigned current_time) noexcept {
    if (tl.empty()) return nullptr;

    task_t *next_task = nullptr;
    unsigned next_task_timepoint; // = next_task->from_timepoint - current_time + next_task->ticks;

    for (auto &task : tl) {
        if (current_time - task.from_timepoint < task.ticks ) { // task is in sleep
            unsigned this_task_timepoint = task.from_timepoint - current_time + task.ticks;
            if (next_task == nullptr || this_task_timepoint < next_task_timepoint) {
                next_task_timepoint = this_task_timepoint;
                next_task = &task;
            }
        }
    }
    return next_task;
}

void __attribute__((interrupt(TIMER0_A0_VECTOR))) timer_a0_layer::isr() {
    for (auto &task : timer_a0::waiting_tasks_) {
        if (current_time() - task.from_timepoint >= task.ticks) {
            scheduler::unblock(task.nr);
        }
    }

    for (auto next_task = timer_a0::find_next_ready_task(timer_a0::waiting_tasks_, wakeup_time());
        next_task != nullptr;
        next_task = timer_a0::find_next_ready_task(timer_a0::waiting_tasks_, wakeup_time())) {
        
        wakeup_time(next_task->from_timepoint + next_task->ticks);

        if (current_time() - next_task->from_timepoint < next_task->ticks) {
            break; // did not miss interrupt :)
        }

        // we might missed interrupt: set next task and unblock current task manually
        scheduler::unblock(next_task->nr);
        // TODO: rename unblock to resume
    }

    // wakeup scheduler
    __bic_SR_register_on_exit(LPM0_bits);
}

} // namespace uos
