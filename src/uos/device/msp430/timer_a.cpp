#include <uos/device/msp430/timer_a.hpp>

namespace uos::dev::msp430 {

#ifdef UOS_DEV_MSP430_ENABLE_TIMERA0
void __attribute__((interrupt(TIMER0_A0_VECTOR))) timer_a0_layer::isr() {
    timer_a0::handle_isr();

    // wakeup scheduler
    __bic_SR_register_on_exit(LPM0_bits);
}
#endif

} // namespace uos
