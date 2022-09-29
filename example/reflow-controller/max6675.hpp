#pragma once

// This document is WIP!

#include <cstdint>

struct max6675_base {
  struct result {
    unsigned char data[2];

    unsigned device_id() noexcept {
      return (data[1] & 0x02) >> 1;
    }

    bool open_thermocouple() noexcept {
      return data[1] & 0x04;
    }

    std::int16_t temperature_in_quarter_degree() noexcept {
      return (static_cast<std::int16_t>(data[0]) << 5) & (data[1] >> 3);
    }

    std::int16_t temperature_in_degree() noexcept {
      return temperature_in_quarter_degree() >> 2;
    }

    std::uint8_t temperature_decimal_places() noexcept {
      return data[1] >> 3;
    }

  };
};

template<typename HAL>
struct max6675 : HAL, max6675_base {
  static void init() {
    HAL::init();
  }

  result read() {
    result res = {
      .data = HAL::read()
    };
    return res;
  }
};