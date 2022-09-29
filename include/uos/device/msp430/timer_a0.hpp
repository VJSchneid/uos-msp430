#pragma once

#include <uos/device/msp430/timer_a.hpp>

namespace uos::dev::msp430 {

struct timer_a0_hal {
    static inline uint16_t wakeup_time() noexcept {
        return TA0CCR0;
    }

    static inline uint16_t current_time() noexcept {
        return TA0R;
    }

    static inline void wakeup_time(uint16_t timestamp) noexcept {
        TA0CCR0 = timestamp;
    }

    static inline void enable_timer() noexcept {
        TA0CCTL0 = CCIE;
        TA0CTL = TA0CTL | MC__CONTINUOUS;
    }

    enum class clock_source : uint16_t {
      TAxCLK = TASSEL__TACLK,
      ACLK = TASSEL__ACLK,
      SMCLK = TASSEL__SMCLK,
      INCLK = TASSEL__INCLK
    };

    enum class clock_divider : uint16_t {
      div_1 = ID__1,
      div_2 = ID__2,
      div_4 = ID__4,
      div_8 = ID__8
    };

    static inline void clock(clock_source clk_src, clock_divider div) noexcept {
      TA0CTL = (TA0CTL & ~0x03C0) | static_cast<uint16_t>(clk_src) | static_cast<uint16_t>(div);
    }

    static inline void stop_timer() noexcept {
        TA0CTL = TA0CTL & ~MC_3;
    }

    static inline bool running() noexcept {
        return TA0CTL & MC__CONTINUOUS;
    }

    static void __attribute__((interrupt(TIMER0_A0_VECTOR))) isr();
};

using timer_a0 = timer_a<timer_a0_hal, scheduler>;


}