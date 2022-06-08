#pragma once

#include <uos/detail/autoselect.hpp>

namespace uos::dev {

struct tm1628a_base {
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

    enum class display_enable_t : unsigned char { on = 0b1000, off = 0b0000 };
};

tm1628a_base::pulse_width_t &operator++(tm1628a_base::pulse_width_t &v) {
    v = static_cast<tm1628a_base::pulse_width_t>((static_cast<unsigned char>(v) + 1) & 0x7);
    return v;
}

tm1628a_base::pulse_width_t &operator++(tm1628a_base::pulse_width_t &v, int) {
    return ++v;
}

tm1628a_base::pulse_width_t &operator--(tm1628a_base::pulse_width_t &v) {
    v = static_cast<tm1628a_base::pulse_width_t>((static_cast<unsigned char>(v) - 1) & 0x7);
    return v;
}

tm1628a_base::pulse_width_t &operator--(tm1628a_base::pulse_width_t &v, int) {
    return --v;
}

/// SerialInterface must support 3 different types of transfers:
/// - command write
/// - command + data write
/// - command + data read
template<typename SerialInterface>
struct tm1628a : tm1628a_base {
    static constexpr void init() {
        SerialInterface::init();
    }

    static constexpr void display_mode(display_mode_t mode) {
        unsigned char cmd = static_cast<unsigned char>(command_t::display_mode) |
                            static_cast<unsigned char>(mode);
        SerialInterface::write(cmd);
    }

    static constexpr void prepare_write(bool address_increment = true) {
        unsigned char cmd = static_cast<unsigned char>(command_t::data) |
                            static_cast<unsigned char>(data_dir_t::write) |
                            static_cast<unsigned char>(address_increment ? address_inc_t::on
                                                                         : address_inc_t::off);
        SerialInterface::write(cmd);
    }

    static constexpr void display_control(pulse_width_t brightness, bool display_enable) {
        unsigned char cmd = static_cast<unsigned char>(command_t::display_control) |
                            static_cast<unsigned char>(brightness) |
                            static_cast<unsigned char>(display_enable ? display_enable_t::on :
                                                                        display_enable_t::off);
        SerialInterface::write(cmd);
    }

    /**
     * Write single byte to display memory
     */
    static constexpr void write(unsigned char address, unsigned char data) {
        write(address, &data, 1);
    }

    static constexpr void write(unsigned char address, unsigned char const *data,
                                unsigned length) {
        unsigned char cmd = static_cast<unsigned char>(command_t::address) |
                            (address & 0xf);

        SerialInterface::write(cmd, data, length);
    }

    /**
     * Read single keyscan byte
     */
    static constexpr unsigned char read(bool address_increment = true) {
        unsigned char data;
        read(&data, 1, address_increment);
        return data;
    }

    static constexpr void read(unsigned char *data, unsigned length,
                               bool address_increment = true) {
        unsigned char cmd = static_cast<unsigned char>(command_t::data) |
                            static_cast<unsigned char>(data_dir_t::read) |
                            static_cast<unsigned char>(address_increment ? address_inc_t::on
                                                                         : address_inc_t::off);
        SerialInterface::read(cmd, data, length);
    }

    static constexpr void address(unsigned char adr) {
        SerialInterface::write(static_cast<unsigned char>(command_t::address) | (adr & 0xf));
    }
};

}