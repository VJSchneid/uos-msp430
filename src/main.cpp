#include <msp430.h>

#include <uos/device/msp430/timer_a.hpp>
#include <uos/device/msp430/gpio.hpp>
#include <uos/scheduler.hpp>
#include <uos/uca1.hpp>
#include <uos/device/msp430/timer_a.hpp>

#include "old_spi.hpp"

unsigned char stack2[512];
unsigned char stack3[512];
void *sp2 = &stack2[512];
void *sp3 = &stack3[512];
void *sp1;

tm1628a<tm1628a_ucb0> *seg_driver;

using timer = uos::dev::msp430::timer;

void main1() {

    tm1628a<tm1628a_ucb0> segment_driver;
    seg_driver = &segment_driver;

    segment_driver.display_mode(display_mode_t::bits_6_segments_11);
    segment_driver.prepare_write();
    segment_driver.display_control(pulse_width_t::_14_of_16, true);

    unsigned char data[14]; // write data to display register, auto increment
    for (unsigned i = 0; i < 13; i += 2) {
        data[i] = 0x00;
        data[i + 1] = 0x00;
    }

    segment_driver.write(0, data, 14);

    max6675_uca0 temp;

    //uos::timer.sleep(5000); // wait a little bit for clock to become stable
    /*uos::uca1.transmit("Starting Reflow Controller...\r\n");
    uos::uca1.transmit("Version: " GIT_VERSION "\r\n");
    uos::uca1.transmit("Build Date: " __DATE__ " " __TIME__ "\r\n");*/

    while (1) {
        auto t = temp.read();

        auto nachkomma = (((t >> 3) & 0x03) * 25 + 5) / 10;

        auto result = print_display((t >> 5) * 10 + nachkomma, 0);

        print_display(result + ((t & 0x7) << 1), 1);
        P3OUT = P3OUT ^ BIT0;
        timer::sleep(60000);
    }
}


void main2() {
    while (!seg_driver) {
        // TODO replace sleep with yield
        timer::sleep(1000);
    }

    pulse_width_t brightness = pulse_width_t::_14_of_16;

    P2IES = 0b111; // toggle on falling edge (btn pushed down)

    seg_driver->display_control(brightness, true);
    while (true) {
        P2IFG = 0; // only listen from now for keychanges (debounce)
        uos::dev::msp430::port2::wait_for_change(0b111);
        uos::uca1.transmit("Button was pressed!\r\n");
        brightness++;
        seg_driver->display_control(brightness, true);
        P3OUT = P3OUT | BIT2;
        timer::sleep(5000);
        P3OUT = P3OUT & ~BIT2;
    }
}

void main3() {
    while(!seg_driver) {
        timer::sleep(1000);
    }

    unsigned char leds = 1;
    bool forwards = true;
    while(true) {
        seg_driver->write(3, (leds >> 0) & 0b11 | (((leds >> 0) & 0b100) << 1));
        seg_driver->write(5, (leds >> 3) & 0b1);
        seg_driver->write(1, (leds >> 4) & 0b11 | (((leds >> 4) & 0b100) << 1));
        timer::sleep(15000);

        if (forwards) {
            leds <<= 1;
        } else {
            leds >>= 1;
        }
        if (leds == 0x40 && forwards || leds == 1 && !forwards) {
            forwards = !forwards;
        }
    }
}

int main() {
    WDTCTL = WDTPW | WDTHOLD;
    __bis_SR_register(GIE);

    // Configure GPIO
    P3OUT = P3OUT & ~(BIT0 | BIT2); // Clear P1.0 output latch
    P3DIR = P3DIR | BIT0 | BIT2;    // For LED

    P2SEL0 = BIT5 | BIT6; // UCA1TXD + UCA1RXD

    // Configure USCI1 for UART 19200 BAUD Transmission
    UCA1CTLW0 = UCSWRST;
    UCA1CTLW0 = UCA1CTLW0 | UCSSEL__SMCLK;
    UCA1IFG = 0; // reset all interrupt flags
    // baudrate = 19200
    UCA1BRW = 3; 
    UCA1MCTLW_H = 0x2;
    UCA1MCTLW_L = UCOS16 | (4 << 4);
    // enable USCI1
    UCA1CTLW0 = UCA1CTLW0 & ~UCSWRST;


    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 = PM5CTL0 & ~LOCKLPM5;

    uos::scheduler::init();

    uos::scheduler::add_task(1, sp2, main2);
    uos::scheduler::add_task(2, sp3, main3);

    main1();
    // panic from here
}
