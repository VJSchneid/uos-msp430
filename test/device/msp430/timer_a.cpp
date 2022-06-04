#include <catch2/catch.hpp>

#include <uos/device/msp430/timer_a.hpp>
#include "test_scheduler.hpp"

using namespace uos::dev::msp430;

static void add_task(timer_a_base<stub_scheduler>::task_list_t &tl,
              std::vector<timer_a_base<stub_scheduler>::task_t> &vec,
              unsigned starting_point,
              unsigned ticks) {
  
  auto &td = vec.emplace_back(tl.create());
  td.starting_timepoint = starting_point;
  td.ticks = ticks;
  tl.prepend(td);
}

TEST_CASE("find_next_ready_task for timer_a using common values", "[msp430][timer]") {
  using timer = timer_a_base<stub_scheduler>;

  std::vector<timer_a_base<stub_scheduler>::task_t> tasks_;
  // reserve enough space to prevent reallocation
  tasks_.reserve(10);

  // create task_list first
  timer::task_list_t tl;
  CHECK(timer::find_next_ready_task(tl, 0) == nullptr);

  add_task(tl, tasks_, 100, 100);
  add_task(tl, tasks_, 100, 50);

  CHECK(timer::find_next_ready_task(tl, 0) == nullptr);
  CHECK(timer::find_next_ready_task(tl, 99) == nullptr);
  CHECK(timer::find_next_ready_task(tl, 100) == &tasks_[1]);
  CHECK(timer::find_next_ready_task(tl, 149) == &tasks_[1]);
  CHECK(timer::find_next_ready_task(tl, 150) == &tasks_[0]);
  CHECK(timer::find_next_ready_task(tl, 199) == &tasks_[0]);
  CHECK(timer::find_next_ready_task(tl, 200) == nullptr);
}

TEST_CASE("find_next_ready_task for timer_a with zero ticks", "[msp430][timer]") {
  using timer = timer_a_base<stub_scheduler>;

  std::vector<timer_a_base<stub_scheduler>::task_t> tasks_;
  // reserve enough space to prevent reallocation
  tasks_.reserve(10);

  timer::task_list_t tl;
  add_task(tl, tasks_, 0, 0);
  add_task(tl, tasks_, 1, 0);

  CHECK(timer::find_next_ready_task(tl, 0) == nullptr);
  CHECK(timer::find_next_ready_task(tl, 1) == nullptr);
  CHECK(timer::find_next_ready_task(tl, 2) == nullptr);
}

TEST_CASE("find_next_ready_task for timer_a with maximal values", "[msp430][timer]") {
  using timer = timer_a_base<stub_scheduler>;

  std::vector<timer_a_base<stub_scheduler>::task_t> tasks_;
  // reserve enough space to prevent reallocation
  tasks_.reserve(10);

  timer::task_list_t tl;
  add_task(tl, tasks_, 0, 0xffff);
  add_task(tl, tasks_, 1, 0xffff);

  CHECK(timer::find_next_ready_task(tl, 0) == &tasks_[0]);
  CHECK(timer::find_next_ready_task(tl, 1) == &tasks_[0]);
  CHECK(timer::find_next_ready_task(tl, 0xfffe) == &tasks_[0]);
  CHECK(timer::find_next_ready_task(tl, 0xffff) == &tasks_[1]);
  CHECK(timer::find_next_ready_task(tl, 0xffff) == &tasks_[1]);

  add_task(tl, tasks_, 0xffff, 10);
  CHECK(timer::find_next_ready_task(tl, 0xfffe) == &tasks_[0]);
  CHECK(timer::find_next_ready_task(tl, 0xffff) == &tasks_[1]);
  CHECK(timer::find_next_ready_task(tl, 0)      == &tasks_[2]);
  CHECK(timer::find_next_ready_task(tl, 10)     == &tasks_[0]);
}

struct timer_test_layer {
  static uint16_t wakeup_time() noexcept {
    return taxccr0;
  }
  
  static uint16_t current_time() noexcept {
    return taxr;
  }

  static void wakeup_time(uint16_t timestamp) noexcept {
    // wakeup time should only be set, when timer is disabled 
    CHECK(!running());
    taxccr0 = timestamp;
  }

  static void enable_timer() noexcept {
    timer_en = true;
  }

  static void stop_timer() noexcept {
    timer_en = false;
  }

  static bool running() noexcept {
    return timer_en;
  }

  static void isr() noexcept;

  static void reset() noexcept {
    taxccr0 = 0;
    taxr = 0;
    timer_en = false;
  }

  static inline uint16_t taxccr0;
  static inline uint16_t taxr;
  static inline bool timer_en;
};

using test_timer = timer_a<timer_test_layer, stub_scheduler>;

void timer_test_layer::isr() noexcept {
  test_timer::handle_isr();
}

TEST_CASE("basic timer_a sleep", "[msp430][timer]") {
  timer_test_layer::reset();
  stub_scheduler::init();

  uint16_t expected_taxccr0;
  uint16_t wakeup_time;
  bool just_suspend_once = true;

  stub_schedule_layer::on_suspension = [&]() {
    REQUIRE(just_suspend_once);
    // timer has to run always, when suspended
    CHECK(timer_test_layer::running());

    REQUIRE(timer_test_layer::taxccr0 == expected_taxccr0);

    timer_test_layer::taxr = wakeup_time;
    timer_test_layer::isr();

    just_suspend_once = false;
  };

  expected_taxccr0 = 1000;
  wakeup_time = 1000;
  test_timer::sleep(1000);
  CHECK(!timer_test_layer::running());

  just_suspend_once = true;
  expected_taxccr0 = 1200;
  wakeup_time = 1210;
  test_timer::sleep(200);
  CHECK(!timer_test_layer::running());

  just_suspend_once = true;
  expected_taxccr0 = 1194;
  wakeup_time = 1200;
  test_timer::sleep(0xfff0);
  CHECK(!timer_test_layer::running());
}