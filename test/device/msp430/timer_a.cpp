#include <catch2/catch.hpp>

#include <uos/device/msp430/timer_a.hpp>
#include "test_scheduler.hpp"

using namespace uos::dev::msp430;

static void add_task(timer_a_base<callback_scheduler>::task_list_t &tl,
              std::vector<timer_a_base<callback_scheduler>::task_t> &vec,
              unsigned starting_point,
              unsigned ticks) {
  
  auto &td = vec.emplace_back(tl.create());
  td.starting_timepoint = starting_point;
  td.ticks = ticks;
  tl.prepend(td);
}

TEST_CASE("find_next_ready_task for timer_a using common values", "[msp430][timer]") {
  using timer = timer_a_base<callback_scheduler>;

  std::vector<timer_a_base<callback_scheduler>::task_t> tasks_;
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
  using timer = timer_a_base<callback_scheduler>;

  std::vector<timer_a_base<callback_scheduler>::task_t> tasks_;
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
  using timer = timer_a_base<callback_scheduler>;

  std::vector<timer_a_base<callback_scheduler>::task_t> tasks_;
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

using test_timer = timer_a<timer_test_layer, callback_scheduler>;

void timer_test_layer::isr() noexcept {
  test_timer::handle_isr();
  callback_scheduler::wakeup_next();
}

TEST_CASE("basic timer_a sleep from single thread", "[msp430][timer]") {
  timer_test_layer::reset();
  callback_scheduler::reset();

  REQUIRE(test_timer::idle());

  uint16_t expected_taxccr0;
  uint16_t wakeup_time;

  callback_scheduler::on_suspend = [&]() {
    // timer has to always run, when suspended
    CHECK(timer_test_layer::running());

    REQUIRE(timer_test_layer::taxccr0 == expected_taxccr0);

    timer_test_layer::taxr = wakeup_time;

    REQUIRE(callback_scheduler::is_blocked(0));
    timer_test_layer::isr();
    // thread must be unblocked after isr
    REQUIRE(!callback_scheduler::is_blocked(0));
  };

  expected_taxccr0 = 1000;
  wakeup_time = 1000;
  test_timer::sleep(1000);
  CHECK(!timer_test_layer::running());

  expected_taxccr0 = 1200;
  wakeup_time = 1210;
  test_timer::sleep(200);
  CHECK(!timer_test_layer::running());

  expected_taxccr0 = 1194;
  wakeup_time = 1200;
  test_timer::sleep(0xfff0);
  CHECK(!timer_test_layer::running());
}

TEST_CASE("timer_a multiple threads sleep (basic)", "[msp430][timer]") {
  timer_test_layer::reset();
  callback_scheduler::reset();

  REQUIRE(test_timer::idle());


  uint16_t expected_taxccr0;
  uint16_t next_taxr;
  callback_scheduler::on_suspend = [&]() {
    // timer has to always run, when suspended
    CHECK(timer_test_layer::running());

    REQUIRE(timer_test_layer::taxccr0 == expected_taxccr0);

    timer_test_layer::taxr = next_taxr;

    if (timer_test_layer::taxccr0 <= next_taxr) {
      timer_test_layer::isr();
    }
  };

  callback_scheduler::threads.resize(3);

  // Thread 0
  callback_scheduler::threads[0].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 0);
      expected_taxccr0 = 0xffff;
      next_taxr = 200;
      test_timer::sleep(0xffff);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == static_cast<uint16_t>(0xffff + 10));
    }
  };

  // Thread 1
  callback_scheduler::threads[1].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 200);
      expected_taxccr0 = 0x1000 + 200;
      next_taxr = 400;
      test_timer::sleep(0x1000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 0x1000 + 300);
      REQUIRE(timer_test_layer::wakeup_time() == 0x2000 + 400);
      // wakeup next thread
      timer_test_layer::taxr = timer_test_layer::wakeup_time();
      timer_test_layer::isr();
    }
  };

  // Thread 2
  callback_scheduler::threads[2].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 400);
      // taxccr0 should not change! (keep old value)
      next_taxr = timer_test_layer::wakeup_time() + 100; // wakeup one thread
      test_timer::sleep(0x2000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 0x2000 + 400);
      REQUIRE(timer_test_layer::wakeup_time() == 0xffff);
      // wakeup next thread
      timer_test_layer::taxr = timer_test_layer::wakeup_time() + 10;
      timer_test_layer::isr();
    }
  };

  callback_scheduler::wakeup_next();
  CHECK(callback_scheduler::complete());
}


TEST_CASE("timer_a resume multiple threads at once", "[msp430][timer]") {
  timer_test_layer::reset();
  callback_scheduler::reset();

  REQUIRE(test_timer::idle());


  uint16_t expected_taxccr0;
  uint16_t next_taxr;
  callback_scheduler::on_suspend = [&]() {
    // timer has to always run, when suspended
    CHECK(timer_test_layer::running());

    REQUIRE(timer_test_layer::taxccr0 == expected_taxccr0);

    timer_test_layer::taxr = next_taxr;

    if (timer_test_layer::taxccr0 <= next_taxr) {
      timer_test_layer::isr();
    }
  };

  callback_scheduler::threads.resize(3);

  // Thread 0
  callback_scheduler::threads[0].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 0);
      expected_taxccr0 = 65000;
      next_taxr = 10;
      test_timer::sleep(65000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 65100);
    }
  };

  // Thread 1
  callback_scheduler::threads[1].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 10);
      // taxccr0 should not change! (keep old value)
      next_taxr = 20;
      test_timer::sleep(65200);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 65220);
    }
  };

  // Thread 2
  callback_scheduler::threads[2].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 20);
      // taxccr0 should not change! (keep old value)
      next_taxr = 65100; // wakeup all threads
      test_timer::sleep(65000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 65100);
      REQUIRE(timer_test_layer::taxccr0 == 65210);
      timer_test_layer::taxr = 65220;
      timer_test_layer::isr();
    }
  };

  callback_scheduler::wakeup_next();
  CHECK(callback_scheduler::complete());
}

TEST_CASE("timer_a resume multiple threads with overflow", "[msp430][timer]") {
  timer_test_layer::reset();
  callback_scheduler::reset();

  REQUIRE(test_timer::idle());

  uint16_t expected_taxccr0;
  uint16_t next_taxr;
  callback_scheduler::on_suspend = [&]() {
    // timer has to always run, when suspended
    CHECK(timer_test_layer::running());

    REQUIRE(timer_test_layer::taxccr0 == expected_taxccr0);

    timer_test_layer::taxr = next_taxr;

    if (timer_test_layer::taxccr0 <= next_taxr) {
      timer_test_layer::isr();
    }
  };

  timer_test_layer::taxr = 60000;
  callback_scheduler::threads.resize(4);

  // Thread 0
  callback_scheduler::threads[0].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 60000);
      expected_taxccr0 = 65000;
      next_taxr = 61000;
      test_timer::sleep(5000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 65100);
      CHECK(timer_test_layer::taxccr0 == 11464);
      timer_test_layer::taxr = 11464 + 100;
      timer_test_layer::isr();
    }
  };

  // Thread 1
  callback_scheduler::threads[1].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 61000);
      // taxccr0 should not change! (keep old value)
      next_taxr = 62000;
      test_timer::sleep(65000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 60464 + 100);
      // reached last sleep
    }
  };

  // Thread 2
  callback_scheduler::threads[2].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 62000);
      // taxccr0 should not change! (keep old value)
      next_taxr = 63000; // wakeup all threads
      test_timer::sleep(15000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 11464 + 100);
      CHECK(timer_test_layer::taxccr0 == 60464);
      timer_test_layer::taxr = 60464 + 100;
      timer_test_layer::isr();
    }
  };
  
  // Thread 3
  callback_scheduler::threads[3].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 63000);
      expected_taxccr0 = 64000;
      next_taxr = 64100;
      test_timer::sleep(1000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 64100);
      CHECK(timer_test_layer::taxccr0 == 65000);
      timer_test_layer::taxr = 65100;
      timer_test_layer::isr();
    }
  };

  callback_scheduler::wakeup_next();
  CHECK(callback_scheduler::complete());
}
