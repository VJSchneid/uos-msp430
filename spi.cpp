#include <msp430.h>

void write_display(const unsigned char *data, unsigned length) {
  P1OUT = P1OUT & ~BIT0; // enable device select
  for (unsigned i = 0; i < length; i++) {
    P3OUT ^= BIT0;
    UCB0TXBUF = data[i];
    while (UCB0STATW & UCBUSY); // wait for transmission
  }
  P1OUT = P1OUT | BIT0; // disable device select
}

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


  unsigned char cmd[] = {static_cast<unsigned char>(0b1100'0000 + (second_display ? 0 : 6)), seg_c};
  write_display(cmd, 2);
  cmd[0] += 2; // next segement address
  cmd[1] = seg_b;
  write_display(cmd, 2);
  cmd[0] += 2; // next segement address
  cmd[1] = seg_a;
  write_display(cmd, 2);
  return true;
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;

    // Configure GPIO
    P3OUT &= ~(BIT0 | BIT2);  // Clear P1.0 output latch
    P3DIR |= BIT0 | BIT2;     // For LED
    P1OUT |= BIT0;
    P1DIR = BIT0;
    P1SEL0 |= BIT1 | BIT2 | BIT3;           // SPI pins

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;

    // Configure USCI_B0 for SPI mode
    UCB0CTLW0 |= UCSWRST;                   // Software reset enabled
    UCB0CTLW0 |= UCMODE_0 | UCMST | UCSYNC | UCCKPL; // SPI mode, Master mode, sync, high idle clock
    UCB0BRW = 0x0008;                       // baudrate = SMCLK / 8
    UCB0CTLW0 &= ~UCSWRST;

        
    const unsigned char display_mode_setting = 0b00'0000'10; // use 6 grids and 11 segments
    write_display(&display_mode_setting, 1);


    const unsigned char data_command_setting = 0b01'000'0'00; // write data to display register, auto increment
    write_display(&data_command_setting, 1);


    unsigned char write[15]; // write data to display register, auto increment
    write[0] = 0b11'00'0000;
    for (unsigned i = 1; i < 14; i+=2) {
      write[i] = 0xff;
      write[i+1] = 0xff;
    } 
    write_display(write, 15);

    const unsigned char display_control_command = 0b10'00'1'111; // display on, max pulse width
    write_display(&display_control_command, 1);



    int brightness = 3;
    int ctr = 0;
    while(1) {
      if (~P2IN & BIT2) {
        brightness = (brightness + 1) % 8;

        unsigned char brightness_cmd = 0b10'00'1'000 + brightness;
        write_display(&brightness_cmd, 1);
        while(~P2IN & BIT2);
      }
      if (~P2IN & BIT1) {
        while(~P2IN & BIT1);
      }
      if (!print_display(ctr, 1)) {
        ctr = -100;
      }
      print_display(ctr, 0);
      ctr += 1;
      P3OUT ^= BIT2;
      __delay_cycles(50000);
    }
}

