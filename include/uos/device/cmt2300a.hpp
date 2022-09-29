#pragma once

namespace uos::dev {

struct cmt2300a_base {
    enum class cus_addr : unsigned char {
        sys2 = 0x0D,
        lbd = 0x5F,
        mode_ctl = 0x60,
        mode_sta = 0x61,
        en_ctl = 0x62,
        io_sel = 0x65,
        int1_ctl = 0x66,
        int2_ctl = 0x67,
        int_en = 0x68,
        fifo_ctl = 0x69,
        int_clr1 = 0x6A,
        int_clr2 = 0x6B,
        fifo_clr = 0x6C,
        int_flag = 0x6D,
        fifo_flag = 0x6E,
        rssi_code = 0x6F,
        rssi_dbm = 0x70,
        lbd_result = 0x71,
        reset = 0x7F
    };

    static constexpr const unsigned char cmt2300a_config[] = {
        0x00, 0x66, 0xEC, 0x1C, 0xF0, 0x80, 0x14, 0x08,
        0x11, 0x02, 0x02, 0x00, 0xAE, 0xE0, 0x45, 0x00,
        0x00, 0xF4, 0x10, 0xE2, 0x42, 0x20, 0x00, 0x81,
        0x42, 0xCF, 0xA7, 0x8C, 0x42, 0xC4, 0x4E, 0x1C,
        0x32, 0x18, 0x10, 0x99, 0xC1, 0x9B, 0x06, 0x0A, 
        0x9F, 0x39, 0x29, 0x29, 0xC0, 0x51, 0x2A, 0x53,
        0x00, 0x00, 0xB4, 0x00, 0x00, 0x01, 0x00, 0x00, 
        0x12, 0x08, 0x00, 0xAA, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xD4, 0x2D, 0x00, 0x0F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x60,
        0xFF, 0x00, 0x00, 0x1F, 0x10, 0x70, 0x93, 0x01,
        0x00, 0x42, 0xB0, 0x00, 0x42, 0x0C, 0x3F, 0x6A
    };

};

template<typename HAL>
struct cmt2300a : public cmt2300a_base, HAL  {
    static void init() {
        HAL::init();

        HAL::write(cus_addr::reset, 0xff); // soft reset CMT2300A
        HAL::sleep_ms(20);

        HAL::write(cus_addr::mode_ctl, 0x02); // go_standby
        // wait for chip to enter standby state
        volatile unsigned char reg;
        do {
            reg = HAL::read(cus_addr::mode_sta);
        } while((reg & 0x0f) != 0x02);

        write_bitmask(cus_addr::mode_sta, 0x10 | 0x20, 0x10); // enable CFG_RETAIN and disable RSTN_IN
        write_bitmask(cus_addr::en_ctl, 0x20, 0x20); // enable LOCKING_EN
        write_bitmask(cus_addr::sys2, 0x80 | 0x40 | 0x20, 0x00); // disable LFOSC

        clear_ifg_flags();

        // configure cmt2300a
        for (unsigned addr = 0; addr < 0x5F; addr++) {
            HAL::write(static_cast<cus_addr>(addr), cmt2300a_config[addr]);
        }

        // output INT1 on gpio2 and INT2 on gpio3
        HAL::write(cus_addr::io_sel, 0x00 | 0x20);

        write_bitmask(cus_addr::int1_ctl, 0x1f, 0x06); // set INT1 to CRC_OK
        write_bitmask(cus_addr::int2_ctl, 0x1f, 0x0A); // set INT2 to TX_DONE

        // enable CRC_OK and TX_DONE interrupts
        HAL::write(cus_addr::int_en, 0x02 | 0x20);

        // disable LFOSC
        write_bitmask(cus_addr::sys2, 0x80 | 0x40 | 0x20, 0x00);

        // apply configuration by going into sleep state
        HAL::write(cus_addr::mode_ctl, 0x10); // go_sleep
        // wait for chip to enter sleep state
        while((HAL::read(cus_addr::mode_sta) & 0x0f) != 0x01);
    }

    static void write_bitmask(cus_addr addr, unsigned char bitmask, unsigned char bits) noexcept {
        auto reg = (HAL::read(addr) & (~bitmask)) | bits;
        HAL::write(addr, reg);
    }

    static void send_fixed_len(unsigned char const *data, unsigned char len) noexcept {
        HAL::write(cus_addr::mode_ctl, 0x02); // go_standby
        // wait for chip to enter standby state
        while((HAL::read(cus_addr::mode_sta) & 0x0f) != 0x02);

        // clear TX_DONE interrupt flag
        //write(cus_addr::int_clr1, 0x04);
        clear_ifg_flags();

        // clear TX FIFO
        HAL::write(cus_addr::fifo_clr, 0x01);
        // FIFO Write and select tx fifo (sel might not be needed)
        HAL::write(cus_addr::fifo_ctl, 0x05);

        // write data to fifo
        HAL::write_fifo(data, len);

        // go into TX state
        HAL::write(cus_addr::mode_ctl, 0x40);

        // wait for TX_DONE flag
        HAL::wait_for_gpio3();
        //while(~HAL::read(cus_addr::int_clr1) & 0x08);

        clear_ifg_flags();

        // go into sleep state
        HAL::write(cus_addr::mode_ctl, 0x10);
        // wait for chip to enter sleep state
        while((HAL::read(cus_addr::mode_sta) & 0x0f) != 0x01);
    }

    static void clear_ifg_flags() noexcept {
        // clear all interrupt flags
        HAL::write(cus_addr::int_clr1, 0x07);
        HAL::write(cus_addr::int_clr2, 0x3f);
    }

    static void receive_fixed_len(unsigned char *data, unsigned char len) noexcept {
        HAL::write(cus_addr::mode_ctl, 0x02); // go_standby
        // wait for chip to enter standby state
        while((HAL::read(cus_addr::mode_sta) & 0x0f) != 0x02);

        // select RX FIFO
        write_bitmask(cus_addr::fifo_ctl, 0x01 | 0x04, 0);
        clear_ifg_flags();
        // clear RX FIFO
        HAL::write(cus_addr::fifo_clr, 0x02);

        // go into RX state
        HAL::write(cus_addr::mode_ctl, 0x08);

        // wait for PKT_DONE
        HAL::wait_for_gpio2();
        //while(~HAL::read(cus_addr::int_flag) & 0x02)

        HAL::write(cus_addr::mode_ctl, 0x02); // go_standby
        // wait for chip to enter standby state
        while((HAL::read(cus_addr::mode_sta) & 0x0f) != 0x02);

        HAL::read_fifo(data, len);

        // go into sleep
        HAL::write(cus_addr::mode_ctl, 0x10);
        // wait for chip to enter sleep state
        while((HAL::read(cus_addr::mode_sta) & 0x0f) != 0x01);
    }
};

}