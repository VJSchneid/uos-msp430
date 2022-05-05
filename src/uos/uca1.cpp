#include <uos/uca1.hpp>

#include <uos/scheduler.hpp>
#include <msp430.h>

namespace uos {


uca1_t uca1;

void uca1_t::transmit(const char *data, unsigned length) {
    // TODO power save and maybe reset with UCSWRST?
    if (length == 0) return;
    scheduler.prepare_block();

    waiting_task my_task;
    my_task.nr = scheduler.taskid();
    my_task.data = data;
    my_task.length = length;

    my_task.next = waiting_tx_;
    waiting_tx_ = &my_task;

    UCA1IE = UCA1IE & ~UCTXCPTIFG; // disable TX interrupts
    if (!(UCA1STATW & UCBUSY) && !(UCA1IFG & UCTXCPTIFG)) {
        //UCA1TXBUF_L = *my_task.data;
        //my_task.data = my_task.data + 1;
        //my_task.length = my_task.length + 1;
        UCA1IFG = UCA1IFG | UCTXCPTIFG;
    }
    UCA1IE = UCA1IE | UCTXCPTIFG; // enable TX interrupts

    scheduler.suspend_me();

    remove_from_list(my_task);
}
/*void uca1_t::transmit(const char *str) {
    unsigned length;
    for (length = 0; str[length] != 0; length++);
    transmit(str, length);
}*/

void uca1_t::remove_from_list(waiting_task &t) {
    waiting_task *task;
    waiting_task *volatile *prev_task_next = &waiting_tx_;
    for (task = waiting_tx_; task != nullptr; task = task->next) {
        if (task == &t) {
            *prev_task_next = t.next;
            return;
        }
        prev_task_next = &task->next;
    }
}

bool uca1_t::has_content(waiting_task *task) {
    for(;task != nullptr; task = task->next) {
        if (task->length > 0) return true;
    }
    return false;
}

uca1_t::waiting_task *uca1_t::get_last_valid(waiting_task *task) {
    if (!task || task->length == 0) return nullptr;
    while(task->next && task->next->length > 0) {
        task = task->next;
    }
    return task;
}

void __attribute__((interrupt(USCI_A1_VECTOR))) uca1_isr() {
    switch(__even_in_range(UCA1IV,18)) {
    case 0x00: // Vector 0: No interrupts
        break;
    case 0x02: // Vector 2: UCRXIFG
        break;
    case 0x04: // Vector 4: UCTXIFG
        break;
    case 0x06: // Vector 6: UCSTTIFG
        break;
    case 0x08: { // Vector 8: UCTXCPTIFG
        auto task = uca1.get_last_valid(uca1.waiting_tx_);
        if (!task) break;
        UCA1TXBUF_L = *task->data;
        task->data = task->data + 1;
        task->length = task->length - 1;
        if (task->length == 0) {
            scheduler.unblock(task->nr);
            // wakeup scheduler
            __bic_SR_register_on_exit(LPM0_bits);
        }
        break;
    }
    default: break;
    }
}

}