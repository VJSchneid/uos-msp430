#include <catch2/catch.hpp>
#include <uos/basic_scheduler.hpp>
#include <functional>

struct callback_scheduler {
  static void unblock(unsigned tasknr) {
    REQUIRE(tasknr < threads.size());
    threads[tasknr].blocked--;
    if (on_unblock) on_unblock(tasknr);
  }

  static bool is_blocked(unsigned tasknr) {
    REQUIRE(tasknr < threads.size());
    return threads[tasknr].blocked > 0;
  }

  static void prepare_suspend() {
    if (on_prepare_suspend) on_prepare_suspend();
  }

  static unsigned taskid() {
    return current_taskid;
  }

  static void suspend_me() {
    REQUIRE(current_taskid < threads.size());
    threads[current_taskid].blocked++;

    if (on_suspend) on_suspend();
    wakeup_next();
  }

  static void reset() {
    current_taskid = 0;
    on_unblock = {};
    on_prepare_suspend = {};
    on_suspend = {};
    threads = { {} };
  }

  static void wakeup_next() {
    int thread_ctr = 0;
    for (auto &thread : threads) {
      while (thread.blocked == 0 && !thread.calls.empty()) {
        current_taskid = thread_ctr;
        auto call = std::move(thread.calls.front());
        thread.calls.erase(thread.calls.begin());
        call();
      }
      thread_ctr++;
    }
  }

  static bool complete() {
    for (auto &thread : threads) {
      if (!thread.calls.empty()) {
        return false;
      }
    }
    return true;
  }

  static inline unsigned current_taskid = 0;

  static inline std::function<void(unsigned)> on_unblock;
  static inline std::function<void()> on_prepare_suspend;
  static inline std::function<void()> on_suspend;

  struct thread_t {
    std::vector<std::function<void()>> calls;
    int blocked = 0;
  };

  static inline std::vector<thread_t> threads;
};