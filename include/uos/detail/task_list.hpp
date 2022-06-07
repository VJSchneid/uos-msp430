#pragma once

namespace uos {

template<typename TaskData, typename Scheduler>
struct task_list {
    struct task_t : TaskData {
        // all fields that are accessed by ISR must
        // be marked volatile to prevent invalid content
        volatile unsigned nr;
        task_t * volatile next;
    };

    void clear() noexcept {
        waiting_tasks_ = nullptr;
    }

    // TODO variadic argument to create TaskData
    task_t create() {
        task_t my_task;
        my_task.nr = Scheduler::taskid();
        return my_task;
    }

    void prepend(task_t &my_task) noexcept {
        my_task.next = waiting_tasks_;
        waiting_tasks_ = &my_task;
    }

    void remove(task_t &t) noexcept {
        task_t *task;
        task_t *volatile *prev_task_next = &waiting_tasks_;
        for (task = waiting_tasks_; task != nullptr; task = task->next) {
            if (task == &t) {
                *prev_task_next = t.next;
                return;
            }
            prev_task_next = &task->next;
        }
    }

    bool empty() noexcept {
        return waiting_tasks_ == nullptr;
    }

    struct iterator {
        void operator++() noexcept {
            task_ = task_->next;
        }

        task_t *operator->() noexcept {
            return task_;
        }

        task_t &operator*() noexcept {
            return *task_;
        }

        bool operator==(iterator const &rhs) const noexcept {
            return task_ == rhs.task_;
        }

        bool operator!=(iterator const &rhs) const noexcept {
            return !(*this == rhs);
        }

        task_t *volatile task_;
    };

    iterator begin() noexcept {
        return {waiting_tasks_};
    }

    iterator end() noexcept {
        return {nullptr};
    }

    task_t * volatile waiting_tasks_ = nullptr;
};

}