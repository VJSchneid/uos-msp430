#pragma once

#include <msp430.h>

extern "C" {

void thread_switch(void *volatile *new_sp, void *volatile *old_sp) noexcept;

}

namespace uos::dev::msp430 {

void thread_init(void **new_sp, void (*initial_function)()) noexcept;

/// MCU Layer for msp430 scheduler
struct scheduler {
  void thread_switch(void *volatile *new_sp, void *volatile *old_sp) noexcept {
    return thread_switch(new_sp, old_sp);
  }

  void thread_init(void **new_sp, void (*initial_function)()) noexcept {
    thread_init(new_sp, initial_function);
  }

  void sleep_until_interrupt() noexcept {
    __bis_SR_register(LPM0_bits);
  }
};

}