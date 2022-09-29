#include <stdint.h>

namespace uos::dev {

struct measurement_data {
  uint16_t temperature;
  uint16_t humidity;
};


template<typename HAL>
struct hdc1080 : HAL {
  static void init() noexcept {
    HAL::init();
    unsigned char config_reg[] =  {0x10, 0x00};
    HAL::write(0x02, config_reg, 2); // configure acquisition parameters
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