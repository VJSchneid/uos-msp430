#include <uos/device/msp430/eusci.hpp>

namespace uos::dev::msp430 {

void __attribute__((interrupt(USCI_A1_VECTOR))) eusci_a1_layer::isr() {
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
        auto task = eusci_a1::get_active_task(eusci_a1::waiting_tx_tasks_);
        if (!task) break;
        UCA1TXBUF_L = *task->data;
        task->data = task->data + 1;
        task->length = task->length - 1;
        if (task->length == 0) {
            scheduler::unblock(task->nr);
            // wakeup scheduler
            __bic_SR_register_on_exit(LPM0_bits);
        }
        break;
    }
    default: break;
    }
}

}