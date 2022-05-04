#include <uos/pin2.hpp>

#include <uos/scheduler.hpp>

namespace uos {


pin2_t pin2;

void pin2_t::wait_for_change(unsigned char mask) {
    if (!mask) return;
    scheduler.prepare_block();

    waiting_task my_task;
    my_task.nr = scheduler.taskid();
    my_task.mask = mask;

    my_task.next = waiting_tasks_;
    waiting_tasks_ = &my_task;

    // set interrupt enable with bitmask
    P2IE = P2IE | mask;

    scheduler.suspend_me();

    remove_from_list(my_task);
}

void pin2_t::remove_from_list(waiting_task &t) {
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

void __attribute__((interrupt(PORT2_VECTOR))) port2_isr() {
    unsigned char ifgs = P2IFG;
    P2IFG = 0;

    unsigned char next_mask = 0;
    for (auto task = pin2.waiting_tasks_; task != nullptr; task = task->next) {
        if (ifgs & task->mask) {
            scheduler.unblock(task->nr);
        } else {
            next_mask |= task->mask;
        }
    }

    P2IE = next_mask;

    // wakeup scheduler
    __bic_SR_register_on_exit(LPM0_bits);
}

}