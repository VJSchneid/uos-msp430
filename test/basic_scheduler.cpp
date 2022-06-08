#include <catch2/catch.hpp>

#include <uos/basic_scheduler.hpp>
#include <functional>

using namespace uos;

struct test_scheduler_layer {
  static void disable_interrupts() { ie = false; }

  static void enable_interrupts() { ie = true; }

  static void sleep_until_interrupt() {
    // ensure that interrupt are disabled when entering
    // this function to prevent stalling by an interrupt race conditions
    CHECK(!ie);
    ie = true;
    // on sleep is required to unblock threads and prevent a deadlock
    REQUIRE(on_sleep);
    on_sleep();
  }

  static void thread_init(void **new_sp, void (*initial_function)()) {}

  static void thread_switch(void *volatile *new_sp, void *volatile *old_sp) {
    if (on_thread_switch) {
      on_thread_switch();
    }
  }

  static inline bool ie = true;

  static inline std::function<void()> on_sleep;
  static inline std::function<void()> on_thread_switch;
};

using test_scheduler = basic_scheduler<test_scheduler_layer>;

TEST_CASE("basic_scheduler make sure that interrupts are disabled when entering sleep", "[scheduler]") {
  test_scheduler::init();
  int sleep_called = 0;
  test_scheduler_layer::on_sleep = [&](){
    REQUIRE(++sleep_called == 1);
    test_scheduler::unblock(0);
  };
  test_scheduler::suspend_me();

  CHECK(sleep_called == 1);
}

TEST_CASE("basic_scheduler enable interrupts before switching threads", "[scheduler]") {
  test_scheduler::init();

  // on_sleep is not required to be set here, as it should not be called anyway

  int thread_switch_called = 0;
  test_scheduler_layer::on_thread_switch = [&]() {
    thread_switch_called++;
    CHECK(test_scheduler::taskid() == 1);
    CHECK(test_scheduler_layer::ie);
  };

  test_scheduler::add_task(1, nullptr, nullptr);

  test_scheduler::suspend_me();

  CHECK(thread_switch_called == 1);
}