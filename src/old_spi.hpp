#include <msp430.h>

void write_display(const unsigned char *data, unsigned length) {
  P1OUT = P1OUT & ~BIT0; // enable device select
  for (unsigned i = 0; i < length; i++) {
      P3OUT = P3OUT ^ BIT0;
      UCB0TXBUF = data[i];
      while (UCB0STATW & UCBUSY)
          ; // wait for transmission
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

enum class command_t : unsigned char {
    display_mode = 0b00'000000,
    data = 0b01'000000,
    display_control = 0b10'000000,
    address = 0b11'000000
};

enum class display_mode_t : unsigned char {
    bits_4_segments_13 = 0b00,
    bits_5_segments_12 = 0b01,
    bits_6_segments_11 = 0b10,
    bits_7_segments_10 = 0b11
};

enum class data_dir_t : unsigned char { read = 0b00, write = 0b10 };

enum class address_inc_t : unsigned char { on = 0b000, off = 0b100 };

enum class pulse_width_t : unsigned char {
    _1_of_16 = 0b000,
    _2_of_16 = 0b001,
    _3_of_16 = 0b010,
    _10_of_16 = 0b011,
    _11_of_16 = 0b100,
    _12_of_16 = 0b101,
    _13_of_16 = 0b110,
    _14_of_16 = 0b111
};

pulse_width_t &operator++(pulse_width_t &v) {
    v = static_cast<pulse_width_t>((static_cast<unsigned char>(v) + 1) & 0x7);
    return v;
}

pulse_width_t &operator++(pulse_width_t &v, int) {
    v = static_cast<pulse_width_t>((static_cast<unsigned char>(v) + 1) & 0x7);
    return v;
}

template<typename Interface>
struct tm1628a : private Interface {
  using interface_t = Interface;

  enum class display_enable_t : unsigned char { on = 0b1000, off = 0b0000 };

  void display_mode(display_mode_t mode) {
      unsigned char cmd = static_cast<unsigned char>(command_t::display_mode) |
                          static_cast<unsigned char>(mode);
      interface_t::write(cmd);
  }

  void prepare_write(bool address_increment = true) {
      unsigned char cmd =
          static_cast<unsigned char>(command_t::data) |
          static_cast<unsigned char>(data_dir_t::write) |
          static_cast<unsigned char>(address_increment ? address_inc_t::on
                                                       : address_inc_t::off);

      interface_t::write(cmd);
  }

  /**
   * Write single byte to display memory
   */
  void write(unsigned char address, unsigned char data) {
      write(address, &data, 1);
  }

  void write(unsigned char address, unsigned char const *data,
             unsigned length) {
      unsigned char cmd =
          static_cast<unsigned char>(command_t::address) | (address & 0xf);
      interface_t::write(cmd, data, length);
  }

  /**
  * Read single keyscan byte
  */
  unsigned char read(bool address_increment = true) {
      unsigned char data;
      read(&data, 1, address_increment);
      return data;
  }

  void read(unsigned char *data, unsigned length,
            bool address_increment = true) {
      unsigned char cmd =
          static_cast<unsigned char>(command_t::data) |
          static_cast<unsigned char>(data_dir_t::read) |
          static_cast<unsigned char>(address_increment ? address_inc_t::on
                                                       : address_inc_t::off);
      interface_t::read(cmd, data, length);
  }

  void display_control(pulse_width_t brightness, bool display_enable) {
    interface_t::write(
      static_cast<unsigned char>(command_t::display_control) |
      static_cast<unsigned char>(brightness) |
      static_cast<unsigned char>(display_enable ? display_enable_t::on : display_enable_t::off));
  }

  void address(unsigned char adr) {
    interface_t::write(static_cast<unsigned char>(command_t::address) | (adr & 0xf));
  }
};

struct tm1628a_ucb0 {
    tm1628a_ucb0() {
        P1OUT = P1OUT | BIT0;
        P1DIR = P1DIR | BIT0;
        P1SEL0 = P1SEL0 | BIT1 | BIT2 | BIT3; // SPI pins

        // Configure USCI_B0 for SPI mode
        UCB0CTLW0 = UCB0CTLW0 | UCSWRST; // Software reset enabled
        UCB0CTLW0 = UCB0CTLW0 | UCMODE_0 | UCMST | UCSYNC |
                    UCCKPL;  // SPI mode, Master mode, sync, high idle clock
        UCB0BRW = 0x0001;    // baudrate = SMCLK / 8
        UCB0CTLW0 = UCB0CTLW0 & ~UCSWRST;
    }

    void write(const unsigned char cmd) {
        select();
        UCB0TXBUF = cmd;
        while (UCB0STATW & UCBUSY)
            ; // wait for transmission
        deselect();
    }

    void write(const unsigned char cmd, const unsigned char *data,
               unsigned length) {
        select();
        UCB0TXBUF = cmd;
        while (UCB0STATW & UCBUSY)
            ;
        for (unsigned i = 0; i < length; i++) {
            UCB0TXBUF = data[i];
            while (UCB0STATW & UCBUSY)
                ; // wait for transmission
        }
        deselect();
    }

    void read(unsigned char cmd, unsigned char *data, unsigned length) {
        select();
        UCB0TXBUF = cmd;
        while (UCB0STATW & UCBUSY)
            ; // wait for transmission
        for (unsigned i = 0; i < length; i++) {
            UCB0TXBUF = 0xff;
            while (UCB0STATW & UCBUSY)
                ; // wait for transmission
            data[i] = UCB0RXBUF;
        }
        deselect();
    }

private:
    void select() {
        P1OUT = P1OUT & ~BIT0; // enable device select
    }

    void deselect() {
        P1OUT = P1OUT | BIT0; // disable device select
    }
};

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