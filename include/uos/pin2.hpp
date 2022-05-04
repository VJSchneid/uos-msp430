

namespace uos {

struct pin2_t {
    struct waiting_task {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content
        volatile unsigned nr;
        volatile unsigned mask;
        waiting_task * volatile next;
    };

    void wait_for_change(unsigned char mask);

    waiting_task * volatile waiting_tasks_ = nullptr;

private:
    void remove_from_list(waiting_task &t);
};

extern pin2_t pin2;

}