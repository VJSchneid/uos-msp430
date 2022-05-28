#include <uos/device/msp430/scheduler.hpp>

namespace uos::dev::msp430 {

struct timer_base {

    struct waiting_task {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content
        volatile unsigned nr;
        volatile unsigned ticks;
        volatile unsigned from_timepoint;
        waiting_task * volatile next = nullptr;
    };

    void sleep(unsigned ticks) noexcept;

    bool is_someone_waiting() noexcept;

    waiting_task *volatile waiting_tasks_ = nullptr;
    unsigned epoch_ = 0;

    waiting_task *find_next_ready_task(unsigned current_time) noexcept;
private:

    void atomic_insert(waiting_task &t) noexcept;
    void atomic_remove(waiting_task &t) noexcept;

    void check_start() noexcept;
    void check_stop() noexcept;
};

}