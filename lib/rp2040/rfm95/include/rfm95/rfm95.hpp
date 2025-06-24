/// @file rfm95.hpp
/// @brief RFM95 LoRa module driver header file for the rp2040 platform.

#ifndef RFM95_RFM95_HPP
#define RFM95_RFM95_HPP

#include "rfm95/registers.hpp"
#include "rfm95/spi_utils.hpp"

#include <expected>
#include <hardware/spi.h>
#include <pico/stdlib.h>
#include <stdint.h>
#include <string>

namespace rfm95 {

class RFM95Error {
public:
  RFM95Error(std::string message) : message(std::move(message)) {}
  std::string message;
};

class RFM95 {
public:
  /// @brief Construct a new RFM95 driver instance.
  /// @param spi Pointer to the SPI peripheral instance.
  /// @param cs_pin Chip select GPIO pin number.
  RFM95(spi_inst_t *spi, std::size_t cs_pin);

  /// @brief Read the current device mode from the RFM95.
  /// @return std::expected containing the mode register value or an error.
  std::expected<uint8_t, RFM95Error> read_device_mode();

  /// @brief Write a new device mode to the RFM95.
  /// @param mode The desired operation mode.
  /// @return std::expected indicating success or error.
  std::expected<void, RFM95Error> write_device_mode(OpModes mode);

  /// @brief Set the LoRa spreading factor.
  /// @param sf The spreading factor to set.
  /// @return std::expected indicating success or error.
  std::expected<void, RFM95Error> write_spreading_factor(SpreadingFactors sf);

  /// @brief Set the LoRa signal bandwidth.
  /// @param bw The bandwidth to set.
  /// @return std::expected indicating success or error.
  std::expected<void, RFM95Error> write_bandwidth(SignalBandwidth bw);

  /// @brief Set the LoRa coding rate.
  /// @param cr The coding rate to set.
  /// @return std::expected indicating success or error.
  std::expected<void, RFM95Error> write_coding_rate(CodingRate cr);

private:
  SPICpp spi_;
};

} // namespace rfm95

#endif