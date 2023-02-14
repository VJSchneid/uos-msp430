#pragma once

#include <stdint.h>

namespace uos::dev {

struct hdc1080_base {
  struct measurement_data {
    uint16_t temperature;
    uint16_t humidity;
  };
};

template<typename HAL>
struct hdc1080 : HAL, hdc1080_base {
  static void init() noexcept {
    HAL::init();
    // TODO make this uniform for calls to HAL::write(unsigned char*, unsigned);
    // maybe split HAL::write internally to HAL::write_epilog() HAL::write_mid(buffer) HAL::write_prolog()
    unsigned char buffer[2] = {0x10, 0x00};
    HAL::write(0x02, buffer, 2); // configure acquisition parameters
  }

  static void trigger_measurement() noexcept {
    HAL::write(0x00, nullptr, 0);
  }
  static measurement_data read_measurement() noexcept {
    unsigned char data[4];
    HAL::read(data, 4);
    measurement_data result;
    result.temperature = data[0] << 8 | data[1];
    result.humidity = data[2] << 8 | data[3];
    return result;
  }

  static uint16_t manufacturer_id() noexcept {
    unsigned char data[2];
    HAL::write(0xFE, data, 0);
    HAL::read(data, 2);
    return data[0] << 8 | data[1];
  }


  static uint16_t device_id() noexcept {
    unsigned char data[2];
    HAL::write(0xFF, data, 0);
    HAL::read(data, 2);
    return data[0] << 8 | data[1];
  }

};

}