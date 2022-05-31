#include <uos/device/msp430/timer_a.hpp>

namespace uos::dev::msp430 {

timer_a_base::task_t *timer_a_base::find_next_ready_task(task_list<task_data> &tl, unsigned current_time) noexcept {
    if (tl.empty()) return nullptr;

    task_t *next_task = nullptr;
    unsigned min_ticks_until_trigger = 0xffff;

    for (auto &task : tl) {
        unsigned ticks_until_trigger = task.starting_timepoint + task.ticks - current_time - 1;
        if (ticks_until_trigger <= min_ticks_until_trigger) {
            next_task = &task;
            min_ticks_until_trigger = ticks_until_trigger;
        }
    }
    return next_task;
}

#ifdef UOS_DEV_MSP430_ENABLE_TIMERA0
void __attribute__((interrupt(TIMER0_A0_VECTOR))) timer_a0_layer::isr() {
    timer_a0::handle_isr();

    // wakeup scheduler
    __bic_SR_register_on_exit(LPM0_bits);
}
#endif

} // namespace uos
