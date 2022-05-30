#include <uos/device/msp430/eusci.hpp>

namespace uos::dev::msp430 {

#ifdef UOS_DEV_MSP430_ENABLE_EUSCIA1
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
#endif

#ifdef UOS_DEV_MSP430_ENABLE_EUSCIB0
void __attribute__((interrupt(USCI_B0_VECTOR))) eusci_b0_layer::isr() {
    switch(__even_in_range(UCB0IV,18)) {
    case 0x00: // Vector 0: No interrupts
        break;
    case 0x02: // Vector 2: UCRXIFG
        break;
    case 0x04: // Vector 4: UCTXIFG
        break;
    case 0x06: // Vector 6: UCSTTIFG
        break;
    case 0x08: { // Vector 8: UCTXCPTIFG
        auto task = eusci_b0::get_active_task(eusci_b0::waiting_tx_tasks_);
        if (!task) break;
        UCB0TXBUF_L = *task->data;
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
#endif

}