#pragma once

#include <uos/basic_scheduler.hpp>

#ifdef UOS_ARCH_MSP430

#include <msp430.h>

extern "C" {

void thread_switch_asm(void *volatile *new_sp, void *volatile *old_sp) noexcept;

}

namespace uos {

namespace dev::msp430 {

/// MCU Layer for msp430 scheduler
struct scheduler_layer {
  static void thread_switch(void *volatile *new_sp, void *volatile *old_sp) noexcept {
    return thread_switch_asm(new_sp, old_sp);
  }

  static void thread_init(void **new_sp, void (*initial_function)()) noexcept;

  static void enable_interrupts() noexcept {
    __bis_SR_register(GIE);
  }

  static void disable_interrupts() noexcept {
    __bic_SR_register(GIE);
  }

  static void sleep_until_interrupt() noexcept {
    __bis_SR_register(GIE | LPM0_bits);
  }
};

}

using scheduler = basic_scheduler<dev::msp430::scheduler_layer>;

}

#endif