#include <catch2/catch.hpp>

#include <uos/device/msp430/timer_a.hpp>
#include "test_scheduler.hpp"

using namespace uos::dev::msp430;

TEST_CASE("timer_a task: is_expired_in", "[msp430][timer]") {
  using task = timer_a_base<callback_scheduler>::task_data;

  task t;
  t.wakeup_time = 15000;

  CHECK(not t.is_expired_in(0, 1));
  CHECK(not t.is_expired_in(0, 1000));
  CHECK(not t.is_expired_in(0, 5000));
  CHECK(not t.is_expired_in(0, 10000));
  CHECK(not t.is_expired_in(0, 14999));
  CHECK(t.is_expired_in(0, 15000));
  CHECK(t.is_expired_in(0, 20000));
  CHECK(t.is_expired_in(0, 50000));
  CHECK(t.is_expired_in(0, 0xffff));
  CHECK(t.is_expired_in(0, 0xffff));
  CHECK(not t.is_expired_in(0, 0)); // do not interpret this as a complete clock turn

  CHECK(not t.is_expired_in(10000, 10001));
  CHECK(not t.is_expired_in(10000, 14999));
  CHECK(t.is_expired_in(10000, 15000));
  CHECK(t.is_expired_in(10000, 0xffff));
  CHECK(t.is_expired_in(10000, 0));
  CHECK(not t.is_expired_in(10000, 10000)); // do not interpret this as a complete clock turn

  CHECK(t.is_expired_in(14999, 15000));
  CHECK(t.is_expired_in(14999, 0xffff));
  CHECK(t.is_expired_in(14999, 0));
  CHECK(not t.is_expired_in(14999, 14999)); // do not interpret this as a complete clock turn

  CHECK(not t.is_expired_in(15000, 15000)); // do not interpret this as a complete clock turn
  CHECK(not t.is_expired_in(15000, 15001));
  CHECK(not t.is_expired_in(15000, 0xffff));
  CHECK(not t.is_expired_in(15000, 0));
  CHECK(not t.is_expired_in(15000, 14999));

  CHECK(not t.is_expired_in(0xffff, 0));
  CHECK(not t.is_expired_in(0xffff, 14999));
  CHECK(t.is_expired_in(0xffff, 15000));
  CHECK(t.is_expired_in(0xffff, 0xfffe));
  CHECK(not t.is_expired_in(0xffff, 0xffff)); // do not interpret this as a complete clock turn

  t.wakeup_time = 55892;
  CHECK(not t.is_expired_in(3146, 5892));
  CHECK(not t.is_expired_in(5892, 6309));
  CHECK(not t.is_expired_in(5892, 30000));
  CHECK(not t.is_expired_in(5892, 55891));
  CHECK(not t.is_expired_in(0xffff, 55891));
  CHECK(not t.is_expired_in(55896, 55891));
  CHECK(t.is_expired_in(55896, 55892));
  CHECK(t.is_expired_in(5892, 55892));

  t.wakeup_time = 19628;
  CHECK(t.is_expired_in(19557, 19663));
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
  test_timer::init();

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
  test_timer::init();

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
  test_timer::init();

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
  test_timer::init();

  REQUIRE(test_timer::idle());


  uint16_t expected_taxccr0;
  uint16_t next_taxr;
  callback_scheduler::on_suspend = [&]() {
    // timer has to always run, when suspended
    CHECK(timer_test_layer::running());

    REQUIRE(timer_test_layer::taxccr0 == expected_taxccr0);

    uint16_t time_after_isr = next_taxr - timer_test_layer::taxccr0;
    uint16_t time_step = next_taxr - timer_test_layer::taxr;
    bool call_isr = time_step > time_after_isr;

    timer_test_layer::taxr = next_taxr;

    if (call_isr) {
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


TEST_CASE("issue timer_a sleep slightly before and after an active timer expires", "[msp430][timer]") {
  timer_test_layer::reset();
  callback_scheduler::reset();
  test_timer::init();

  REQUIRE(test_timer::idle());

  uint16_t expected_taxccr0;
  uint16_t next_taxr;
  callback_scheduler::on_suspend = [&]() {
    // timer has to always run, when suspended
    CHECK(timer_test_layer::running());

    CHECK(timer_test_layer::taxccr0 == expected_taxccr0);

    uint16_t time_after_isr = next_taxr - timer_test_layer::taxccr0;
    uint16_t time_step = next_taxr - timer_test_layer::taxr;
    bool call_isr = time_step > time_after_isr;

    timer_test_layer::taxr = next_taxr;

    if (call_isr) {
      timer_test_layer::isr();
    }
  };

  timer_test_layer::taxr = 10280;
  callback_scheduler::threads.resize(2);

  // Thread 0
  callback_scheduler::threads[0].calls = {
    [&]() {
      CHECK(timer_test_layer::taxr == 10280);
      expected_taxccr0 = 60280;
      next_taxr = 60173; // slightly before this sleep expires
      test_timer::sleep(50000);
    },
    [&]() {
      CHECK(timer_test_layer::taxr == 60280 + 500);
      CHECK(timer_test_layer::taxccr0 == 37637);
      expected_taxccr0 = 52640;
      next_taxr = 37700;
      timer_test_layer::taxr = 37640; // slightly after the sleep from thread 1 expires
      test_timer::sleep(15000);
    },
    []() {
      CHECK(timer_test_layer::taxr == 52640+500);
      CHECK(timer_test_layer::taxccr0 == 53640);
      timer_test_layer::taxr = 53640 + 500;
      timer_test_layer::isr();
    }
  };

  // Thread 1
  callback_scheduler::threads[1].calls = {
    [&]() {
      CHECK(timer_test_layer::taxr == 60173);
      CHECK(expected_taxccr0 == 60280); // keep old expected_taxccr0
      next_taxr = expected_taxccr0 + 500;
      test_timer::sleep(43000);
    },
    [&]() { // after sleep
      CHECK(timer_test_layer::taxr == 37700);
      CHECK(timer_test_layer::taxccr0 == 52640);
      timer_test_layer::taxr = timer_test_layer::taxccr0; // set exactly to taxccr0
      expected_taxccr0 = 53640;
      next_taxr = 52640 + 500;
      test_timer::sleep(1000);
    },
    []() {
      CHECK(timer_test_layer::taxr == 53640 + 500);
    }
  };

  callback_scheduler::wakeup_next();
  CHECK(callback_scheduler::complete());
}

TEST_CASE("issue timer_a sleep slightly after multiple active timers expired", "[msp430][timer]") {
  timer_test_layer::reset();
  callback_scheduler::reset();
  test_timer::init();

  REQUIRE(test_timer::idle());


  uint16_t expected_taxccr0;
  uint16_t next_taxr;
  callback_scheduler::on_suspend = [&]() {
    // timer has to always run, when suspended
    CHECK(timer_test_layer::running());

    CHECK(timer_test_layer::taxccr0 == expected_taxccr0);

    uint16_t time_after_isr = next_taxr - timer_test_layer::taxccr0;
    uint16_t time_step = next_taxr - timer_test_layer::taxr;
    bool call_isr = time_step > time_after_isr;

    timer_test_layer::taxr = next_taxr;

    if (call_isr) {
      timer_test_layer::isr();
    }
  };

  timer_test_layer::taxr = 0;
  callback_scheduler::threads.resize(3);

  // Thread 0
  callback_scheduler::threads[0].calls = {
    [&]() {
      CHECK(timer_test_layer::taxr == 0);
      expected_taxccr0 = 10000;
      next_taxr = 500; // slightly before this sleep expires
      test_timer::sleep(10000);
    },
    []() {
      CHECK(timer_test_layer::taxr == 10500);
    }
  };

  // Thread 1
  callback_scheduler::threads[1].calls = {
    [&]() {
      CHECK(timer_test_layer::taxr == 500);
      CHECK(expected_taxccr0 == 10000); // keep old expected_taxccr0
      next_taxr = 1000;
      test_timer::sleep(10000);
    },
    []() {
      CHECK(timer_test_layer::taxr == 10500);
    }
  };

  // Thread 2
  callback_scheduler::threads[2].calls = {
    [&]() {
      CHECK(timer_test_layer::taxr == 1000);
      timer_test_layer::taxr = 10500; // both previous timer expired now
      expected_taxccr0 = 20500;
      next_taxr = 10500;
      test_timer::sleep(10000);
    }
  };

  callback_scheduler::wakeup_next();
  CHECK(callback_scheduler::complete());
}

TEST_CASE("timer_a reproduce issue", "[msp430][timer]") {
  timer_test_layer::reset();
  callback_scheduler::reset();
  test_timer::init();

  REQUIRE(test_timer::idle());

  uint16_t expected_taxccr0;
  uint16_t next_taxr;
  callback_scheduler::on_suspend = [&]() {
    // timer has to always run, when suspended
    CHECK(timer_test_layer::running());

    CHECK(timer_test_layer::taxccr0 == expected_taxccr0);

    uint16_t time_after_isr = next_taxr - timer_test_layer::taxccr0;
    uint16_t time_step = next_taxr - timer_test_layer::taxr;
    bool call_isr = time_step > time_after_isr;

    timer_test_layer::taxr = next_taxr;

    if (call_isr) {
      timer_test_layer::isr();
    }
  };

  timer_test_layer::taxr = 45939;
  callback_scheduler::threads.resize(2);

  // Thread 0
  callback_scheduler::threads[0].calls = {
    [&]() {
      CHECK(timer_test_layer::taxr == 45939);
      expected_taxccr0 = 60939;
      next_taxr = timer_test_layer::taxr + 8196; // = 54135
      test_timer::sleep(15000);
    },
    [&]() { // after sleep
      CHECK(timer_test_layer::taxr == 60939 + 1000);
      CHECK(timer_test_layer::taxccr0 == 28599);
      expected_taxccr0 = 11403;
      next_taxr = expected_taxccr0 + 1500;
      test_timer::sleep(15000);
    },
    [&]() { // after sleep
      CHECK(timer_test_layer::taxr == 11403 + 1500);
      CHECK(timer_test_layer::taxccr0 == 28599);
      expected_taxccr0 = 27903;
      next_taxr = expected_taxccr0 + 1000; // = 28903 => this should resume both sleeps
      test_timer::sleep(15000);
    },
    [&]() { // after sleep
      CHECK(timer_test_layer::taxr == 27903+1000);
      CHECK(!callback_scheduler::is_blocked(1)); // both sleeps should be resumed
      expected_taxccr0 = 43903;
      test_timer::sleep(15000);
    },
    [&]() {
      CHECK(timer_test_layer::taxr == 44403);
      //CHECK(timer_test_layer::taxccr0 == 3367);
      expected_taxccr0 = 59403;
      next_taxr = expected_taxccr0 + 1000;
      test_timer::sleep(15000);
    },
    []() {
      CHECK(timer_test_layer::taxr == 60403);
    }
  };

  // Thread 1
  callback_scheduler::threads[1].calls = {
    [&]() {
      CHECK(timer_test_layer::taxr == 54135);
      CHECK(expected_taxccr0 == 60939); // keep old expected_taxccr0
      next_taxr = expected_taxccr0 + 1000;
      test_timer::sleep(40000);
    },
    [&]() { // after sleep
      CHECK(timer_test_layer::taxr == 28903);
      CHECK(timer_test_layer::taxccr0 == 43903);
      CHECK(expected_taxccr0 == 43903);
      next_taxr = expected_taxccr0 + 500;
      test_timer::sleep(40000);
    }
  };

  callback_scheduler::wakeup_next();
  CHECK(callback_scheduler::complete());
}

TEST_CASE("timer_a sleep with maximal ticks and isr is called after expiration from a previous sleep", "[msp430][timer]") {
  timer_test_layer::reset();
  callback_scheduler::reset();
  test_timer::init();

  REQUIRE(test_timer::idle());

  uint16_t expected_taxccr0;
  uint16_t next_taxr;
  callback_scheduler::on_suspend = [&]() {
    // timer has to always run, when suspended
    CHECK(timer_test_layer::running());

    REQUIRE(timer_test_layer::taxccr0 == expected_taxccr0);

    uint16_t time_after_isr = next_taxr - timer_test_layer::taxccr0;
    uint16_t time_step = next_taxr - timer_test_layer::taxr;
    bool call_isr = time_step > time_after_isr;

    timer_test_layer::taxr = next_taxr;

    if (call_isr) {
      timer_test_layer::isr();
    }
  };

  timer_test_layer::taxr = 0;
  callback_scheduler::threads.resize(3);

  // Thread 0
  callback_scheduler::threads[0].calls = {
    [&]() {
      expected_taxccr0 = 0xffff;
      next_taxr = 0xf000;
      test_timer::sleep(0xffff);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 0x0400);
    }
  };

  // Thread 1
  callback_scheduler::threads[1].calls = {
    [&]() {
      REQUIRE(timer_test_layer::current_time() == 0xf000);
      expected_taxccr0 = 0xfff0;
      next_taxr = 0x0400;
      test_timer::sleep(0x0ff0);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 0x0400);
    }
  };

  callback_scheduler::wakeup_next();
  CHECK(callback_scheduler::complete());
}

TEST_CASE("timer_a isr is called after multiple later sleeps expired", "[msp430][timer]") {
  timer_test_layer::reset();
  callback_scheduler::reset();
  test_timer::init();

  REQUIRE(test_timer::idle());

  uint16_t expected_taxccr0;
  uint16_t next_taxr;
  callback_scheduler::on_suspend = [&]() {
    // timer has to always run, when suspended
    CHECK(timer_test_layer::running());

    REQUIRE(timer_test_layer::taxccr0 == expected_taxccr0);

    bool call_isr = next_taxr >= timer_test_layer::taxccr0 &&
                    timer_test_layer::taxr <= timer_test_layer::taxccr0;

    timer_test_layer::taxr = next_taxr;

    if (call_isr) {
      timer_test_layer::isr();
    }
  };

  timer_test_layer::taxr = 0;
  callback_scheduler::threads.resize(4);

  // Thread 0
  callback_scheduler::threads[0].calls = {
    [&]() {
      expected_taxccr0 = 10000;
      next_taxr = 0;
      test_timer::sleep(10000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 10100);
    }
  };

  // Thread 1
  callback_scheduler::threads[1].calls = {
    [&]() {
      REQUIRE(timer_test_layer::taxr == 0);
      expected_taxccr0 = 10000;
      next_taxr = 10;
      test_timer::sleep(10000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 10100);
    }
  };

  // Thread 2
  callback_scheduler::threads[2].calls = {
    [&]() {
      REQUIRE(timer_test_layer::taxr == 10);
      expected_taxccr0 = 10000;
      next_taxr = 10;
      test_timer::sleep(10000);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 10100);;
    }
  };

  // Thread 3
  callback_scheduler::threads[3].calls = {
    [&]() {
      REQUIRE(timer_test_layer::taxr == 10);
      expected_taxccr0 = 9999;
      next_taxr = 10100;
      test_timer::sleep(9989);
    },
    []() { // after sleep
      CHECK(timer_test_layer::taxr == 10100);
    }
  };

  callback_scheduler::wakeup_next();
  CHECK(callback_scheduler::complete());
}
