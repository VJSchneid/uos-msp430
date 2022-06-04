#include <catch2/catch.hpp>

#include <uos/detail/task_list.hpp>
#include "../device/msp430/test_scheduler.hpp"

using namespace uos;

struct task_data {
  int extra_data = 123;
};

static std::vector<task_list<task_data, stub_scheduler>::task_t> tasks_;

TEST_CASE("prepend created tasks in task_list", "[task_list]") {
  task_list<task_data, stub_scheduler> tl;

  REQUIRE(tl.empty());

  // allocate correct size to prevent reallocation
  tasks_.resize(4);
  for (int x = 0; x < 4; x++) {
    // set values to check if create sets these values
    tasks_[x].nr = 100;
    tasks_[x].next = tasks_.data();

    tasks_[x] = tl.create();
    // create must set nr field
    CHECK(tasks_[x].nr == 0);
    
    tasks_[x].extra_data = x;
    tl.prepend(tasks_[x]);
    // prepend must set next field
    CHECK(tasks_[x].next == (x > 0 ? &tasks_[x-1] : nullptr));

    int expected_extra_data = x;
    for (auto &task : tl) {
      CHECK(task.extra_data == expected_extra_data);
      expected_extra_data--;
    }
  }
}