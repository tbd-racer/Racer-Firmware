/// @file rfm95.hpp
/// @brief RFM95 LoRa module driver header file for the rp2040 platform.

#ifndef RFM95_RFM95_HPP
#define RFM95_RFM95_HPP

#include "rfm95/registers.hpp"
#include "rfm95/spi_utils.hpp"

#include <hardware/spi.h>
#include <optional>
#include <pico/stdlib.h>
#include <span>
#include <stdint.h>

namespace rfm95 {

enum class TransmitStateMachine { IDLE, WAITING_FOR_TX_DONE };

class RFM95 {
public:
  using RecieveCallback = std::function<void(std::span<const uint8_t>)>;

  enum class RadioState {
    IDLE,
    TRANSMITTING,
    RECEIVING,
  };

  /// @brief Construct a new RFM95 driver instance.
  /// @param spi Pointer to the SPI peripheral instance.
  /// @param cs_pin Chip select GPIO pin number.
  RFM95(spi_inst_t *spi, std::size_t cs_pin,
        RecieveCallback on_recieve_callback = nullptr);

  /// @brief Read the current device mode from the RFM95.
  /// @return The mode register value.
  OpModes read_device_mode() const;

  /// @brief Set a new device mode to the RFM95.
  /// @param mode The desired operation mode.
  /// @return Reference to this for fluid interface.
  RFM95 &set_device_mode(OpModes mode) const;

  /// @brief Set the LoRa spreading factor.
  /// @param sf The spreading factor to set.
  /// @return Reference to this for fluid interface.
  RFM95 &set_spreading_factor(SpreadingFactors sf) const;

  /// @brief Set the LoRa signal bandwidth.
  /// @param bw The bandwidth to set.
  /// @return Reference to this for fluid interface.
  RFM95 &set_bandwidth(SignalBandwidth bw) const;

  /// @brief Set the LoRa coding rate.
  /// @param cr The coding rate to set.
  /// @return Reference to this for fluid interface.
  RFM95 &set_coding_rate(CodingRate cr) const;

  /// @brief Set the LoRa header mode.
  /// @param mode The header mode to set (explicit or implicit).
  /// @return Reference to this for fluid interface.
  RFM95 &set_header_mode(ImplicitHeaderMode mode) const;

  /// @brief Configure the device for long range mode.
  /// @param mode The long range mode to set (FSK/OOK or LoRa).
  /// @return Reference to this for fluid interface.
  RFM95 &set_long_range_mode(LongRangeMode mode) const;

  /// @brief Set the frequency.
  /// @param frequency The frequency to set.
  /// @return Reference to this for fluid interface.
  RFM95 &set_frequency(uint32_t frequency) const;

  /// @brief Initializes the FIFO buffer for transmission filling.
  /// @param data The data to fill into the FIFO.
  TransmitStateMachine transmit(const std::span<const uint8_t> data);

  void enable_continuous_recieve();

  /// @brief Interrupt callback function to handle RFM95 interrupts.
  /// This function should be registered with the GPIO interrupt system.
  /// It will be called when the RFM95 module generates an interrupt.
  void interrupt_callback();

  /// @brief Get the current operation mode of the RFM95.
  /// TODO: update this class to track the current mode internally
  /// instead of reading it from the device every time.
  /// @return The current operation mode of the RFM95.
  OpModes current_mode() const;

private:
  void handle_recieve_interrupt(const RegIrqFlags::Flags &irq_flags);

  size_t read_fifo_buffer();

  SPICpp spi_;

  RecieveCallback on_receive_callback_;

  bool tx_done_signal_{false};

  bool receive_enabled_{false};

  std::array<uint8_t, 256> rx_fifo_buffer_;
};

} // namespace rfm95

#endif // RFM95_RFM95_HPP