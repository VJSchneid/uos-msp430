#include <msp430.h>

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // Stop watchdog timer

    P3OUT &= ~BIT0;                         // Clear P1.0 output latch for a defined power-on state
    P3DIR |= BIT0;                          // Set P1.0 to output direction

    P2DIR &= ~BIT2;                         // Set P1.3 as input

    PM5CTL0 &= ~LOCKLPM5;                   // Disable the GPIO power-on default high-impedance mode
                                            // to activate previously configured port settings

    while (1)                               // Test P1.3
    {
        if (P1IN & BIT3)
            P1OUT |= BIT0;                  // if P1.3 set, set P1.0
    else
        P1OUT &= ~BIT0;                     // else reset
    }
}
