#include "rfm95/rfm95.hpp"
#include <pico/rand.h>
#include <string>
#include <utility>

namespace rfm95 {

RFM95::RFM95(spi_inst_t *spi, std::size_t cs_pin) : spi_(spi, cs_pin) {}

std::expected<uint8_t, RFM95Error> RFM95::read_device_mode() {
  // Read the device mode register
  return spi_.read_register(RegOpMode).transform_error([](SPIErrorCode err) {
    return RFM95Error("Failed to read device mode due to SPI error");
  });
} // namespace rfm95

std::expected<void, RFM95Error> RFM95::write_device_mode(OpModes mode) {
  const uint8_t mode_value = std::to_underlying(mode);

  return spi_.write_register(RegOpMode, mode_value)
      .transform_error([]([[maybe_unused]] SPIErrorCode err) {
        return RFM95Error("Failed to write device mode due to SPI error");
      });
}

std::expected<void, RFM95Error>
RFM95::write_spreading_factor(SpreadingFactors sf) {
  return spi_
      .update_register(rfm95::RegModemConfig2::addr,
                       [&](uint8_t value) {
                         return rfm95::RegModemConfig2::set_spreading_factor(
                             value, sf);
                       })
      .transform_error([](SPIErrorCode) {
        return RFM95Error("Failed to write spreading factor");
      });
}

std::expected<void, RFM95Error> RFM95::write_bandwidth(SignalBandwidth bw) {
  return spi_
      .update_register(rfm95::RegModemConfig1::addr,
                       [&](uint8_t value) {
                         return rfm95::RegModemConfig1::set_bandwidth(value,
                                                                      bw);
                       })
      .transform_error(
          [](SPIErrorCode) { return RFM95Error("Failed to write bandwidth"); });
}

std::expected<void, RFM95Error> RFM95::write_coding_rate(CodingRate cr) {
  return spi_
      .update_register(rfm95::RegModemConfig1::addr,
                       [&](uint8_t value) {
                         return rfm95::RegModemConfig1::set_coding_rate(value,
                                                                        cr);
                       })
      .transform_error([](SPIErrorCode) {
        return RFM95Error("Failed to write coding rate");
      });
}

} // namespace rfm95
