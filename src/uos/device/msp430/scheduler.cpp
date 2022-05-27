#include <uos/device/msp430/scheduler.hpp>

#include <msp430.h>

#ifdef MSP430_LARGE_MODEL
#define MSP430_REGISTER_SIZE 4
#else
#define MSP430_REGISTER_SIZE 2
#endif

#define SP_REGISTER_OFFSET (7 * MSP430_REGISTER_SIZE) // 7 registers (R4-R10) with 4 bytes (large mode) and 2 bytes (normal mode)
#define SP_SUB(sp, n) reinterpret_cast<unsigned char *&>(sp) -= n
#define SP_PUSH(sp, v)                                                         \
    SP_SUB(sp, sizeof(v));                                                     \
    *reinterpret_cast<decltype(v) *>(sp) = v

extern "C" {

void panic() {
    // thread exits
    for (;;) {
        P3OUT = P3OUT ^ BIT2;
        __delay_cycles(200000);
    }
}

}

namespace uos::dev::msp430 {

void thread_init(void **new_sp, void (*initial_function)()) noexcept {
    SP_PUSH(*new_sp, &panic);
    SP_PUSH(*new_sp, initial_function);
    SP_SUB(*new_sp, SP_REGISTER_OFFSET);
}

}