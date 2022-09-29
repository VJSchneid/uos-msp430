#include <uos/device/msp430/timer_a0.hpp>

namespace uos::dev::msp430 {

void __attribute__((interrupt(TIMER0_A0_VECTOR))) timer_a0_hal::isr() {
    timer_a0::handle_isr();

    // wakeup scheduler
    __bic_SR_register_on_exit(LPM0_bits);
}

} // namespace uos
