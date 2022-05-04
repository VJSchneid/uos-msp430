#include <msp430.h>

#include <uos/timer.hpp>
#include <uos/scheduler.hpp>

unsigned char stack2[1024];
void *sp2 = &stack2[1024];
void *sp1;

void main1() {
    while (true) {
        for (int x = 0; x < 20; x++) {
            P3OUT = P3OUT ^ BIT0;
            //uos::scheduler.unblock(1);
            //uos::scheduler.suspend_me();
            //__delay_cycles(100000);
            //uos::scheduler.unblock(1);
            uos::timer.sleep(60000);
        }
    }
}

void main2() {
    while (true) {
        //uos::scheduler.suspend_me();
        P3OUT = P3OUT ^ BIT2;
        //uos::timer.sleep(10000);
        //thread_switch(sp1, &sp2);
        //uos::scheduler.suspend_me();
        uos::timer.sleep(60000);
    }
}

int main() {
    WDTCTL = WDTPW | WDTHOLD;
    __bis_SR_register(GIE);

    // Configure GPIO
    P3OUT = P3OUT & ~(BIT0 | BIT2); // Clear P1.0 output latch
    P3DIR = P3DIR | BIT0 | BIT2;    // For LED

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 = PM5CTL0 & ~LOCKLPM5;

    uos::scheduler.add_task(1, sp2, main2);
    //thread_init(&sp2, &main2);
    //thread_switch(sp2, &sp1);

    main1();
    // panic from here
}
