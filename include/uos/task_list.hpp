namespace uos {

template<typename TaskData, typename Scheduler>
struct task_list {
    struct waiting_task : TaskData {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content
        volatile unsigned nr;
        waiting_task * volatile next;
    };

    // TODO create iterators

    // TODO variadic argument to create TaskData
    waiting_task create() {
        waiting_task my_task;
        my_task.nr = Scheduler::taskid();
        return my_task;
    }

    void prepend(waiting_task &my_task) {
        my_task.next = waiting_tasks_;
        waiting_tasks_ = &my_task;
    }

    void remove(waiting_task &t) {
        waiting_task *task;
        waiting_task *volatile *prev_task_next = &waiting_tasks_;
        for (task = waiting_tasks_; task != nullptr; task = task->next) {
            if (task == &t) {
                *prev_task_next = t.next;
                return;
            }
            prev_task_next = &task->next;
        }
    }

    waiting_task * volatile waiting_tasks_ = nullptr;
};

}