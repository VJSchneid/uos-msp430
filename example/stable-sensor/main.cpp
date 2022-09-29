#include <msp430.h>

#include <uos/device/msp430/timer_a0.hpp>
#include <uos/device/msp430/gpio.hpp>
#include <uos/detail/autoselect.hpp>
#include <uos/device/cmt2300a.hpp>
#include <uos/device/hdc1080.hpp>
#include <array>


struct hdc1080_eusci_b {
  static inline void init() noexcept {
    P1SEL0 = BIT2 | BIT3;
    UCB0CTLW0 = UCB0CTLW0 | UCSWRST; // reset eUSCI_B
    UCB0CTLW0 = UCSWRST | UCMST | UCMODE_3 | UCSYNC | UCSSEL__SMCLK;
    UCB0BRW = 10; // set frequency to 50KHz
    UCB0CTLW0 = UCB0CTLW0 & ~UCSWRST; // clear reset
    UCB0I2CSA = 0b1000000;
  }

  static inline void write(unsigned char address, unsigned char const *data, unsigned char length) noexcept {
    UCB0IFG   = UCB0IFG & ~UCTXIFG0; // clear UCTXIFG0
    UCB0CTLW0 = UCB0CTLW0 | UCTR; // set transmit mode
    START:
    UCB0CTLW0 = UCB0CTLW0 | UCTXSTT; // send START condition
    while(~UCB0IFG & UCTXIFG0); // wait for UCB0TXBUF ready
    UCB0TXBUF = address; // send address to pointer register
    for (unsigned char x = 0; x < length; x++) {
      while(~UCB0IFG & UCTXIFG0) { // wait for UCB0TXBUF to get transfered into shift register
        if(UCB0IFG & UCNACKIFG) {
          goto START;
        }
      }
      UCB0TXBUF = data[x];
    }
    while(~UCB0IFG & UCTXIFG0) { // wait for UCB0TXBUF to get transfered into shift register
      if(UCB0IFG & UCNACKIFG) {
        goto START;
      }
    }
    UCB0CTLW0 = UCB0CTLW0 | UCTXSTP; // send STOP condition
    while(UCB0CTLW0 & UCTXSTP); // wait for completion
  }

  static inline void write(unsigned char address, unsigned char data) {
    write(address, &data, 1);
  }

  static inline unsigned char read(unsigned char *data, unsigned char length) noexcept {
    if (length == 0) return 0;

    (void)UCB0RXBUF; // clear UCRXIFG0
    UCB0CTLW0 = UCB0CTLW0 & ~UCTR; // set receive mode
    //RETRY:
    UCB0CTLW0 = UCB0CTLW0 | UCTXSTT; // send START condition
    for (unsigned char x = 0; x < length-1; x++) {
      while(~UCB0IFG & UCRXIFG0) { // wait for new data in UCB0RXBUF
        /*if (~UCB0IFG & UCNACKIFG) {
          goto RETRY;
        }*/
      }
      data[x] = UCB0RXBUF;
    }
    UCB0CTLW0 = UCB0CTLW0 | UCTXSTP; // send STOP condition
    while(~UCB0IFG & UCRXIFG0); // wait for new data in UCB0RXBUF
    data[length-1] = UCB0RXBUF;
    while(UCB0CTLW0 & UCTXSTP); // wait for completion
    return length;
  }

};

struct cmt2300a_chipselect {
    static inline void init() noexcept {
        P3OUT = P3OUT | BIT1;
        P3DIR = P3DIR | BIT1;
    }

    static inline void select() noexcept {
        P3OUT = P3OUT & ~BIT1;
    }

    static inline void deselect() noexcept {
        P3OUT = P3OUT | BIT1;
    }
};

struct cmt2300a_fifoselect {
    static inline void init() noexcept {
        P3DIR = P3DIR | BIT2;
        P3OUT = P3OUT | BIT2;
    }

    static inline void select() noexcept {
        P3OUT = P3OUT & ~BIT2;
    }

    static inline void deselect() noexcept {
        P3OUT = P3OUT | BIT2;
    }
};

using timer = uos::dev::msp430::timer_a0;

#define XT1_TICKS_ms(ms) ((32768/8)*(ms)/1000)
#define XT1_TICKS_us(us) ((32768/8)*(us)/1000000)

struct cmt2300a_uca1_polling {

    using cus_addr = uos::dev::cmt2300a_base::cus_addr;
    static inline void init() noexcept {
        cmt2300a_chipselect::init();
        cmt2300a_fifoselect::init();
        P2SEL0 = P2SEL0 | BIT4 | BIT5 | BIT6; // SPI CLK | RXD | TXD pins
        P2DIR  = P2DIR & ~(BIT6 | BIT2 | BIT3 | BIT7); // high impedance when P2SEL is changed for TXD and set gpio1,2,3 as input
        P2DIR  = P2DIR | BIT4; // set CLK as output to force low
        P2OUT  = P2OUT & ~BIT4; // force low

        // Configure USCI_A0 for SPI mode
        UCA1CTLW0 = UCA1CTLW0 | UCSWRST; // Software reset enabled
        UCA1CTLW0 = UCSWRST | UCCKPH | UCMSB | UCMST | UCMODE_0 | UCSYNC | UCSSEL__SMCLK;
        //UCA1CTLW0 = UCSWRST | UCCKPL | UCMSB | UCMST | UCMODE_0 | UCSYNC | UCSSEL__SMCLK;
        UCA1BRW   = 0x001;    // baudrate = SMCLK / 1
        UCA1CTLW0 = UCA1CTLW0 & ~UCSWRST;
    }

    static unsigned char read(cus_addr addr) noexcept {
        auto read_req = static_cast<unsigned char>(addr) | 0x80; // make sure r/w bit is set
        cmt2300a_chipselect::select();
        // transmit addr
        UCA1TXBUF = read_req;
        while (UCA1STATW & UCBUSY); // wait for transmission
        // read data
        P2SEL0 = P2SEL0 & ~(BIT6 /*| BIT4*/); // disable TXD and force CLK low
        //P2SEL0 = P2SEL0 | BIT4; // set CLK nominal
        UCA1TXBUF = read_req; // issue next transaction
        while (UCA1STATW & UCBUSY); // wait for transmission
        cmt2300a_chipselect::deselect();
        P2SEL0 = P2SEL0 | BIT6; // enable TXD again
        return UCA1RXBUF;
    }

    static void write(cus_addr addr, unsigned char data) noexcept {
        auto write_req = static_cast<unsigned char>(addr) & ~0x80; // make sure r/w bit is not set
        uos::autoselect<cmt2300a_chipselect> as;
        // transmit addr
        UCA1TXBUF = write_req;
        while (UCA1STATW & UCBUSY); // wait for transmission
        // write data
        //P2SEL0 = P2SEL0 & ~(BIT4); // disable TXD and force CLK low
        //P2SEL0 = P2SEL0 | BIT4; // enable TXD again and set CLK nominal
        UCA1TXBUF = data;
        while (UCA1STATW & UCBUSY); // wait for transmission
    }

    static void read_fifo(unsigned char *data, unsigned char len) noexcept {
        P2SEL0 = P2SEL0 & ~BIT6; // disable TXD
        for (unsigned char i = 0; i < len; i++) {
            uos::autoselect<cmt2300a_fifoselect> as;
            UCA1TXBUF = 0;
            while (UCA1STATW & UCBUSY); // wait for transaction
            data[i] = UCA1RXBUF;
        }
        P2SEL0 = P2SEL0 | BIT6; // enable TXD again
    }
    
    static void write_fifo(unsigned char const *data, unsigned char len) noexcept {
        for (unsigned char i = 0; i < len; i++) {
            cmt2300a_fifoselect::select();
            UCA1TXBUF = data[i];
            while (UCA1STATW & UCBUSY); // wait for transaction
            __delay_cycles(3); // wait for >2µs
            cmt2300a_fifoselect::deselect();
            __delay_cycles(5); // wait for >4µs
        }
    }
    
    static void wait_for_gpio1() noexcept {
        while(~P2IN & 0x04);
    }

    static void wait_for_gpio2() noexcept {
        while(~P2IN & 0x08);
    }

    static void wait_for_gpio3() noexcept {
        while(~P2IN & 0x80);
    }

    static void sleep_ms(unsigned ms) noexcept {
        timer::sleep(XT1_TICKS_ms(ms));
    }
};

struct bat_adc {
    static void init() noexcept {
        SYSCFG2 = SYSCFG2 | ADCPCTL1; // set P1.1 to ADC input
        ADCCTL0 = ADCCTL0 | ADCSHT_2; // ADCON, S&H=16 ADC clks
        ADCCTL1 = ADCCTL1 | ADCSHP;   // ADCCLK = MODOSC; sampling timer
        ADCCTL2 = ADCCTL2 | ADCRES_1; // 10bit resolution
        ADCMCTL0 =ADCMCTL0 | ADCINCH_1; // A1 ADC input select; Vref=AVCC
    }

    static uint16_t sample() noexcept {
        ADCCTL0 = ADCCTL0 | ADCON;
        ADCCTL0 = ADCCTL0 | ADCENC | ADCSC;
        do {
            timer::sleep(XT1_TICKS_ms(1));
        } while(~ADCIFG & ADCIFG0);
        ADCCTL0 = ADCCTL0 & ~(ADCON | ADCENC);
        auto adc_raw = ADCMEM0;
        auto &calibration_gain   = *reinterpret_cast<uint16_t*>(0x1A16);
        auto &calibration_offset = *reinterpret_cast<int16_t*>(0x1A18);
        return (static_cast<int32_t>(adc_raw) * calibration_gain + 0x4000) / 0x8000 + calibration_offset;
    }
};

using cmt2300a = uos::dev::cmt2300a<cmt2300a_uca1_polling>;

//static unsigned char data[16] = "Hello World! X\0";
//bool tx_en = true;

inline void blink() {
    timer::sleep(XT1_TICKS_ms(150));
    P3OUT = P3OUT | BIT0;
    timer::sleep(XT1_TICKS_ms(50));
    P3OUT = P3OUT & ~BIT0;
}

inline void blink(unsigned n) {
    while(n--) {
        blink();
    }
}

void panic() {
    while(true) {
        timer::sleep(XT1_TICKS_ms(10000));
        blink(4);
    }
}

using hdc = uos::dev::hdc1080<hdc1080_eusci_b>;

enum send_cmd : uint8_t {
    hello, // 14 Bytes Device ID, last Byte reserved
    assign_id, // 14 Bytes Device ID, last Byte device No.
    sensor_update // 1 Byte Device No.
};

void format_buffer(unsigned char *buf, unsigned char id, hdc::measurement_data const &v, uint16_t battery_voltage) {
    buf[0] = sensor_update;
    buf[1] = id;
    buf[2] = v.temperature >> 8;
    buf[3] = v.temperature & 0xff;
    buf[4] = v.humidity >> 8;
    buf[5] = v.humidity & 0xff;
    buf[6] = battery_voltage >> 8;
    buf[7] = battery_voltage & 0xff;
}

uint8_t fetch_device_no() {
    struct led_keeper {
        led_keeper() {
            // turn LED on
            P3OUT = P3OUT | BIT0;
        }
        ~led_keeper() {
            // turn LED off
            P3OUT = P3OUT & ~BIT0;
        }
    } keep_led;

    uint8_t request[16];
    request[0] = hello;
    std::fill_n(request, 16, 0);
    request[1] = *reinterpret_cast<uint8_t*>(0x1A04);
    request[2] = *reinterpret_cast<uint8_t*>(0x1A05);
    
    request[3] = *reinterpret_cast<uint8_t*>(0x1A0A);
    request[4] = *reinterpret_cast<uint8_t*>(0x1A0B);
    request[5] = *reinterpret_cast<uint8_t*>(0x1A0C);
    request[6] = *reinterpret_cast<uint8_t*>(0x1A0D);
    request[7] = *reinterpret_cast<uint8_t*>(0x1A0E);
    request[8] = *reinterpret_cast<uint8_t*>(0x1A0F);
    request[9] = *reinterpret_cast<uint8_t*>(0x1A10);
    request[10] = *reinterpret_cast<uint8_t*>(0x1A11);
    cmt2300a::send_fixed_len(request, 16);
    
    while(true) {
        uint8_t response[16];
        cmt2300a::receive_fixed_len(response, 16);
        if (response[0] == assign_id) {
            // check unique device id
            bool is_my_id = std::equal(request+1, request+11, response+1);
            if (is_my_id) {
                auto device_no = response[15];
                return device_no;
            }
        }
    }
}

void main1() {

    // TODO error handling!!!
    blink(1);
    cmt2300a::init();
    blink(1);
    hdc::init();
    blink(1);
    bat_adc::init();

    auto manufacturer_id = hdc::manufacturer_id();
    auto device_id = hdc::device_id();
    if (manufacturer_id != 0x5449 || device_id != 0x1050) {
        panic();
    }

    const auto send_period = XT1_TICKS_ms(10000);

    unsigned char send_buffer[16];
    std::fill_n(send_buffer, 0, 16);
    volatile uint16_t battery_voltage;

    auto device_no = fetch_device_no();

    while(true) {
        battery_voltage = bat_adc::sample();
        hdc::trigger_measurement();
        timer::sleep(XT1_TICKS_ms(16));
        auto measurement = hdc::read_measurement();
        format_buffer(send_buffer, device_no, measurement, battery_voltage);
        cmt2300a::send_fixed_len(send_buffer, 16);
        blink(1);
        timer::sleep(send_period);
    }

    /*int x = 0;
    while (1) {
        #ifdef false
        if (tx_en) {
            if (~P3OUT & BIT0) {
                data[13] = '0' + x;
                x = x < 9 ? x+1 : 0;
                cmt2300a::receive_fixed_len(data, 16);
            }
            P3OUT = P3OUT ^ BIT0;
        }
        timer::sleep((32768/blinks_per_sec));
        #else
        //cmt2300a::receive_fixed_len(data, 16);
        //P3OUT = P3OUT | BIT0;

        timer::sleep(XT1_TICKS_ms(1000));
        P3OUT = P3OUT & ~BIT0;
        #endif
    }*/
}


void main2() {
    P1DIR = P1DIR & ~0x01;
    P1IES = P1IES & ~0x01;

    timer::sleep(XT1_TICKS_ms(50));
    P1IFG = P1IFG & ~0x01;
    
    while(true) {
        uos::dev::msp430::port1::wait_for_change(0x01);
        if (P1IN & 0x01) {
            blink(1);
        }
    }
}

unsigned char stack_main2[256];

int main() {
    WDTCTL = WDTPW | WDTHOLD;
    __bis_SR_register(GIE);

    // Configure GPIO
    P3OUT = P3OUT & ~(BIT0); // Clear P1.0 output latch
    P3DIR = P3DIR | BIT0;    // For LED

    
     // output aclk on P2.2
    /*P2SEL1 = BIT2;
    P2DIR = BIT2;*/

    P2SEL0 = BIT1 | BIT0; // enable XIN and XOUT
    CSCTL6 = XT1DRIVE_3 | XT1AUTOOFF; // highes drive strength for startup

    // wait for crystal to settle
    while(CSCTL7 & XT1OFFG) {
        P3OUT = P3OUT | BIT0;
        CSCTL7 = CSCTL7 & ~(XT1OFFG);
    }
    P3OUT = P3OUT & ~BIT0;
    CSCTL6 = XT1DRIVE_0 | XT1AUTOOFF; // reduce drive strength
    CSCTL4 = SELA__XT1CLK; // ACLK comes from XT1CLK

    // disable power for MAX6675K
    P1DIR = P1DIR | BIT4;
    P1OUT = P1OUT & ~BIT4;

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 = PM5CTL0 & ~LOCKLPM5;

    uos::scheduler::init();

    uos::scheduler::add_task(1, stack_main2+256, main2);

    main1();
    // panic from here
}
