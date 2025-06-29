#ifndef RFM95_SPI_UTILS_HPP
#define RFM95_SPI_UTILS_HPP

#include "rfm95/registers.hpp"
#include <functional>
#include <hardware/spi.h>
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
    // printf("Writing byte: 0b%08b\n", value);
    return spi_write_blocking(spi_, &value, 1);
  }

  inline void write_blocking_strict(uint8_t value) const {
    write_blocking(value);
  }

  inline void write_blocking_strict(const std::span<const uint8_t> data) const {
    write_blocking(data);
  }

  inline uint8_t read_byte_strict() const {
    uint8_t byte = 0;
    spi_read_blocking(spi_, 0, &byte, 1);
    return byte;
  }

  inline void write_register(register_t reg, uint8_t value) const {
    uint8_t data[2] = {add_write_bit(reg), value};

    set_chip_select cs{cs_pin_};

    write_blocking_strict(data);
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
  /// @return The number of bytes written (data size + 1 for the register byte).
  ///
  inline size_t write_register(register_t reg,
                               const std::span<const uint8_t> data) const {
    if (data.empty()) {
      printf("Warning: Attempted to write empty data to register 0x%02X\n",
             reg);
      return 0;
    }

    set_chip_select cs{cs_pin_};

    write_blocking_strict(add_write_bit(reg));
    write_blocking_strict(data);
    return data.size() + 1; // +1 for the register byte
  }

  inline uint8_t read_register(register_t reg) const {
    set_chip_select cs{cs_pin_};

    write_blocking_strict(add_read_bit(reg));
    return read_byte_strict();
  }

  inline void read_register_repeated(register_t reg, size_t len,
                                     std::span<uint8_t> buffer) const {
    set_chip_select cs{cs_pin_};

    const auto reg_with_read_bit = add_read_bit(reg);

    spi_read_blocking(spi_, reg_with_read_bit, buffer.data(), len);
  }

  /// @brief Updates a register by reading its current value, applying a
  /// transformation function, and writing it back.
  ///
  /// @param reg The register address to update.
  /// @param functor A function that takes the current register value and
  /// returns the new value.
  ///
  inline void update_register(register_t reg,
                              std::function<uint8_t(uint8_t)> functor) const {
    const auto current_value = read_register(reg);
    const auto new_value = functor(current_value);
    write_register(reg, new_value);

    // printf("Updating Register 0x%02X from 0b%08b to 0b%08b\n", reg,
    //        current_value, new_value);
  }

private:
  spi_inst_t *spi_;
  std::size_t cs_pin_;
};

} // namespace rfm95

#endif // RFM95_SPI_UTILS_HPP