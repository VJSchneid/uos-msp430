#include <msp430.h>

struct max6675_uca0 {
    max6675_uca0() {
        P1OUT = P1OUT | BIT7;
        P1DIR = P1DIR | BIT7;
        P1SEL0 = P1SEL0 | BIT5 | BIT6; // SPI pins

        // Configure USCI_A0 for SPI mode
        UCA0CTLW0 = UCA0CTLW0 | UCSWRST; // Software reset enabled
        UCA0CTLW0 = UCA0CTLW0 | UCMODE_0 | UCMST | UCSYNC | UCSSEL__SMCLK |
                    UCMSB;  // SPI mode, Master mode, sync, high idle clock
        UCA0BRW = 0x0001;   // baudrate = SMCLK / 8
        UCA0CTLW0 = UCA0CTLW0 & ~UCSWRST;
    }

    unsigned read() {
        select();
        unsigned char data[2];
        for (unsigned i = 0; i < 2; i++) {
            UCA0TXBUF = 0xff;
            while (UCA0STATW & UCBUSY)
                ; // wait for transmission
            data[i] = UCA0RXBUF;
        }
        deselect();
        return (data[0] << 8) + data[1];
    }

private:
    void select() {
        P1OUT = P1OUT & ~BIT7; // enable device select
    }

    void deselect() {
        P1OUT = P1OUT | BIT7; // disable device select
    }
};