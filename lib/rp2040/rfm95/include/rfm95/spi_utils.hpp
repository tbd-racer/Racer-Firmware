#ifndef RFM95_SPI_UTILS_HPP
#define RFM95_SPI_UTILS_HPP

#include "rfm95/registers.hpp"
#include <expected>
#include <functional>
#include <hardware/spi.h>
#include <optional>
#include <pico/stdlib.h>
#include <span>
#include <stdint.h>
#include <stdio.h>

constexpr bool check_register_writes = true;

namespace rfm95 {

inline register_t add_write_bit(register_t reg) {
  return reg | 0b10000000; // Set the write bit (bit 7)
}

inline register_t add_read_bit(register_t reg) {
  return reg & 0b01111111; // Clear the write bit (bit 7)
}

enum class SPIErrorCode { WriteFailed, ReadWriteFailed, ReadFailed };

class set_chip_select {
public:
  set_chip_select(std::size_t cs_pin) : cs_pin_(cs_pin) { cs_select(); }

  ~set_chip_select() { cs_deselect(); }

private:
  inline void cs_select() const {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin_, false);
    asm volatile("nop \n nop \n nop");
  }

  inline void cs_deselect() const {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin_, true);
    asm volatile("nop \n nop \n nop");
  }

  std::size_t cs_pin_;
};

class SPICpp {
public:
  SPICpp(spi_inst_t *spi, std::size_t cs_pin) : spi_(spi), cs_pin_(cs_pin) {}

  /// @brief Writes the data to the SPI bus, blocking until the write is
  /// complete.
  /// @param data The data to write.
  /// @return The number of bytes written.
  inline size_t write_blocking(const std::span<const uint8_t> data) const {
    return spi_write_blocking(spi_, data.data(), data.size());
  }

  /// @brief Writes a single byte to the SPI bus, blocking until the write is
  /// complete
  /// @param value the byte to write
  /// @return The number of bytes written.
  inline size_t write_blocking(uint8_t value) const {
    printf("Writing byte: 0b%08b\n", value);
    return spi_write_blocking(spi_, &value, 1);
  }

  inline std::expected<void, SPIErrorCode>
  write_blocking_strict(uint8_t value) const {
    if (write_blocking(value) < 1) {
      return std::unexpected(SPIErrorCode::WriteFailed);
    }
    return {};
  }

  inline std::expected<void, SPIErrorCode>
  write_blocking_strict(const std::span<const uint8_t> data) const {
    if (write_blocking(data) < data.size()) {
      return std::unexpected(SPIErrorCode::WriteFailed);
    }
    return {};
  }

  inline std::expected<uint8_t, SPIErrorCode> read_byte_strict() const {
    uint8_t byte = 0;
    if (spi_read_blocking(spi_, 0, &byte, 1) < 1) {
      return std::unexpected(SPIErrorCode::ReadFailed);
    }
    return byte;
  }

  inline std::expected<void, SPIErrorCode> write_register(register_t reg,
                                                          uint8_t value) const {
    uint8_t data[2] = {add_write_bit(reg), value};

    set_chip_select cs{cs_pin_};

    return write_blocking_strict(data);
  }

  /// @brief Writes data to a specified register over SPI.
  ///
  /// This function writes the provided data to the given register using SPI
  /// communication. It first selects the chip, sends the register address with
  /// the write bit set, then writes the data block. After the operation, it
  /// deselects the chip.
  ///
  /// @param reg The register address to write to.
  /// @param data The data to write to the register as a span of bytes.
  /// @return std::expected<size_t, SPIErrorCode>
  ///         On success, returns the number of bytes written (data size + 1 for
  ///         the register byte). On failure, returns an SPIErrorCode.
  ///
  inline std::expected<size_t, SPIErrorCode>
  write_register(register_t reg, const std::span<const uint8_t> data) const {
    if (data.empty()) {
      return 0;
    }

    set_chip_select cs{cs_pin_};

    return write_blocking_strict(add_write_bit(reg))
        .and_then([&]() { return write_blocking_strict(data); })
        .transform([&]() {
          return data.size() + 1; // +1 for the register byte
        });
  }

  inline std::expected<uint8_t, SPIErrorCode>
  read_register(register_t reg) const {
    set_chip_select cs{cs_pin_};

    return write_blocking_strict(add_read_bit(reg)).and_then([&]() {
      return read_byte_strict();
    });
  }

  inline std::expected<void, SPIErrorCode>
  update_register(register_t reg,
                  std::function<uint8_t(uint8_t)> functor) const {
    // read the regitster
    // update the value using the functor
    // write the register back

    return read_register(reg).transform(functor).and_then(
        [&](uint8_t value) { return write_register(reg, value); });
  }

private:
  spi_inst_t *spi_;
  std::size_t cs_pin_;
};

} // namespace rfm95

#endif // RFM95_SPI_UTILS_HPP