#include <msp430.h>

int main()  {
  WDTCTL = WDTPW + WDTHOLD;

  P3OUT = BIT0 ;
  P3DIR = BIT2 | BIT0;

  PM5CTL0 &= ~LOCKLPM5; // Disable the GPIO power-on default high-impedance mode
                        // to activate previously configured port settings
  for (;;) {
    P3OUT = P3OUT ^ (BIT0 | BIT2);
    for(volatile unsigned i=0; i < 20000; i++) {}
  }
}
