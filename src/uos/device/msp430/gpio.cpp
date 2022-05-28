#include <uos/device/msp430/gpio.hpp>

namespace uos::dev::msp430 {

#ifdef UOS_DEV_MSP430_ENABLE_PORT1
void __attribute__((interrupt(PORT1_VECTOR))) port1_layer::isr() {
    unsigned char ifgs = P1IFG;
    P1IFG = 0;

    unsigned char next_mask = 0;
    for (auto &task : port1::waiting_tasks_) {
        if (ifgs & task.mask) {
            scheduler::unblock(task.nr);
        } else {
            next_mask |= task.mask;
        }
    }

    P1IE = next_mask;

    // wakeup scheduler
    __bic_SR_register_on_exit(LPM0_bits);
}
#endif

#ifdef UOS_DEV_MSP430_ENABLE_PORT2
void __attribute__((interrupt(PORT2_VECTOR))) port2_layer::isr() {
    unsigned char ifgs = P2IFG;
    P2IFG = 0;

    unsigned char next_mask = 0;
    for (auto &task : port2::waiting_tasks_) {
        if (ifgs & task.mask) {
            scheduler::unblock(task.nr);
        } else {
            next_mask |= task.mask;
        }
    }

    P2IE = next_mask;

    // wakeup scheduler
    __bic_SR_register_on_exit(LPM0_bits);
}
#endif



}
