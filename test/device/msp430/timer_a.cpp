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