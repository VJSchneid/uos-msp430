#include <uos/basic_scheduler.hpp>
#include <functional>

struct stub_schedule_layer {
  static void thread_switch(void *volatile *new_sp, void *volatile *old_sp) noexcept {}
  static void thread_init(void **new_sp, void (*initial_function)()) noexcept {}
  static void sleep_until_interrupt() noexcept {
    suspensions++;
    if (on_suspension) on_suspension();
  }

  static inline int suspensions = 0;
  static inline std::function<void()> on_suspension;
};

using stub_scheduler = uos::basic_scheduler<stub_schedule_layer>;