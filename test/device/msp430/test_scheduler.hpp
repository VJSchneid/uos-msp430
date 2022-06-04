#include <uos/basic_scheduler.hpp>

struct stub_schedule_layer {
  static void thread_init(void *volatile *new_sp, void *volatile *old_sp) noexcept {}
  static void thread_switch(void **new_sp, void (*initial_function)()) noexcept {}
  static void sleep_until_interrupt() noexcept { suspensions++; }

  static inline int suspensions = 0;
};

using stub_scheduler = uos::basic_scheduler<stub_schedule_layer>;