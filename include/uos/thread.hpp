#pragma once

extern "C" {

void thread_start(void *new_sp) noexcept;

void thread_switch(void *volatile *new_sp, void *volatile *old_sp) noexcept;

void thread_init(void **new_sp, void (*initial_function)()) noexcept;
}
