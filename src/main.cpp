#include <msp430.h>

#include <uos/device/msp430/timer_a.hpp>
#include <uos/device/msp430/gpio.hpp>
#include <uos/device/msp430/eusci.hpp>
#include <uos/device/tm1628a.hpp>

unsigned char stack2[512];
unsigned char stack3[512];
void *sp2 = &stack2[512];
void *sp3 = &stack3[512];
void *sp1;

#include "old_spi.hpp"

struct tm1628a_chipselect {
    static inline void init() noexcept {
        P1OUT = P1OUT | BIT0;
        P1DIR = P1DIR | BIT0;
    }

    static inline void select() noexcept {
        P1OUT = P1OUT & ~BIT0; // enable device select
    }

    static inline void deselect() noexcept {
        P1OUT = P1OUT | BIT0; // disable device select
    }
};

struct tm1628a_ucb0_polling {
    static inline void init() noexcept {
        tm1628a_chipselect::init();
        P1SEL0 = P1SEL0 | BIT1 | BIT2 | BIT3; // SPI pins

        // Configure USCI_B0 for SPI mode
        UCB0CTLW0 = UCB0CTLW0 | UCSWRST; // Software reset enabled
        UCB0CTLW0 = UCB0CTLW0 | UCMODE_0 | UCMST | UCSYNC |
                    UCCKPL;  // SPI mode, Master mode, sync, high idle clock
        UCB0BRW = 0x0001;    // baudrate = SMCLK / 1
        UCB0CTLW0 = UCB0CTLW0 & ~UCSWRST;
    }

    static void write(unsigned char cmd) noexcept {
        write(cmd, nullptr, 0);
    }

    static void read(unsigned char cmd, unsigned char *data, unsigned length) noexcept {
        uos::autoselect<tm1628a_chipselect> as;
        // transmit cmd
        UCB0TXBUF = cmd;
        while (UCB0STATW & UCBUSY); // wait for transmission
        
        // receive data
        for (unsigned i = 0; i < length; i++) {
            UCB0TXBUF = 0xff;
            while (UCB0STATW & UCBUSY)
                ; // wait for transmission
            data[i] = UCB0RXBUF;
        }
    }

    static void write(unsigned char cmd, unsigned char const *data, unsigned length) noexcept {
        uos::autoselect<tm1628a_chipselect> as;
        // transmit cmd
        UCB0TXBUF = cmd;
        while (UCB0STATW & UCBUSY); // wait for transmission

        // transmit data
        for (unsigned i = 0; i < length; i++) {
            UCB0TXBUF = data[i];
            while (UCB0STATW & UCBUSY); // wait for transmission
        }
    }
};

using tm1628a = uos::dev::tm1628a_base;
using segment_driver = uos::dev::tm1628a<tm1628a_ucb0_polling>;

using timer = uos::dev::msp430::timer_a0;

unsigned char dec_to_seg(unsigned char value, bool zero_enable = true) {
  switch (value) {
    case 0:
      return zero_enable ? 0b0011'1111 : 0b0000'0000;
    case 1:
      return 0b0000'0110;
    case 2:
      return 0b0101'1011;
    case 3:
      return 0b0100'1111;
    case 4:
      return 0b0110'0110;
    case 5:
      return 0b0110'1101;
    case 6:
      return 0b0111'1101;
    case 7:
      return 0b0000'0111;
    case 8:
      return 0b0111'1111;
    case 9:
      return 0b0110'1111;
    default:
      return 0b0100'0000;
  }
}

bool print_display(int number, bool second_display) {
    if (number > 999 || number < -99) return false;

    bool negative = number < 0;
    number = negative ? -number : number;

    int a = number / 100;
    number -= 100*a;
    int b = number / 10;
    number -= 10*b;
    int c = number;
    unsigned char seg_a = negative && b > 0  ? 0b0100'0000 : dec_to_seg(a, false);
    unsigned char seg_b = negative && b == 0 ? 0b0100'0000 : dec_to_seg(b, a > 0);
    unsigned char seg_c = dec_to_seg(c, true);

    unsigned char address = second_display ? 0 : 6;
    segment_driver::write(address, seg_c);
    segment_driver::write(address+2, seg_b);
    segment_driver::write(address+4, seg_a);
    return true;
}

void main1() {
    segment_driver::display_mode(tm1628a::display_mode_t::bits_6_segments_11);
    segment_driver::prepare_write();
    segment_driver::display_control(tm1628a::pulse_width_t::_14_of_16, true);

    unsigned char data[14]; // write data to display register, auto increment
    for (unsigned i = 0; i < 13; i += 2) {
        data[i] = 0x00;
        data[i + 1] = 0x00;
    }

    segment_driver::write(0, data, 14);

    max6675_uca0 temp;

    //uos::timer.sleep(5000); // wait a little bit for clock to become stable
    /*uos::uca1.transmit("Starting Reflow Controller...\r\n");
    uos::uca1.transmit("Version: " GIT_VERSION "\r\n");
    uos::uca1.transmit("Build Date: " __DATE__ " " __TIME__ "\r\n");*/

    int delay = 65200;
    auto timestamp = timer::delay_from_now(delay);
    auto t1 = TA0R;
    while (1) {
        auto t = temp.read();

        auto nachkomma = (((t >> 3) & 0x03) * 25 + 5) / 10;

        auto result = print_display((t >> 5) * 10 + nachkomma, 0);

        //print_display(result + ((t & 0x7) << 1), 1);
        P3OUT = P3OUT ^ BIT0;

        for (int i = 0; i < 8; i++) {
            timer::sleep(timestamp);
            auto t2 = TA0R;
            volatile int diff = (t2-t1-delay);
            if (!print_display(diff < 0 ? -diff : diff, 1) || diff == 5) {
                diff = diff + 1;
                print_display(-1, 1);
            }
            t1 = t2;
            timestamp.refresh(delay);

        }
    }
}


void main2() {

    tm1628a::pulse_width_t brightness = tm1628a::pulse_width_t::_14_of_16;

    P2IES = 0b111; // toggle on falling edge (btn pushed down)

    segment_driver::display_control(brightness, true);
    while (true) {
        P2IFG = 0; // only listen from now for keychanges (debounce)
        uos::dev::msp430::port2::wait_for_change(0b111);
        uos::dev::msp430::eusci_a1::transmit("Button was pressed!\r\n");
        brightness++;
        segment_driver::display_control(brightness, true);
        P3OUT = P3OUT | BIT2;
        timer::sleep(5000);
        P3OUT = P3OUT & ~BIT2;
    }
}

void main3() {
    unsigned char leds = 1;
    bool forwards = true;
    while(true) {
        segment_driver::write(3, (leds >> 0) & 0b11 | (((leds >> 0) & 0b100) << 1));
        segment_driver::write(5, (leds >> 3) & 0b1);
        segment_driver::write(1, (leds >> 4) & 0b11 | (((leds >> 4) & 0b100) << 1));
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

    segment_driver::init();

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 = PM5CTL0 & ~LOCKLPM5;

    uos::scheduler::init();

    uos::scheduler::add_task(1, sp2, main2);
    uos::scheduler::add_task(2, sp3, main3);

    main1();
    // panic from here
}
