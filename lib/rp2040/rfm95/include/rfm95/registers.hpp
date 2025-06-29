#pragma once

#include <stdint.h>
#include <stdio.h>
#include <utility>

namespace rfm95 {
using register_t = uint8_t;

/// @brief The clock frequency of the RFM95 module. Used in frequency
/// calculations.
static constexpr uint32_t clock_frequency = 32e7;
static constexpr double frequency_step = 32e7 / static_cast<double>((1 << 19));

/// @brief Calculate the frequency register value for the RFM95.
/// @param frequency The desired frequency in Hz.
/// @return The frequency register value.
constexpr uint32_t calculate_frequency(double frequency) {
  if (frequency <= 0) {
    return 0;
  }

  // Finds the nearest register value for the given frequency.
  const double frequency_steps = frequency / frequency_step;

  // round to nearest value
  const auto frequency_steps_rounded = [&]() {
    if (frequency_steps - static_cast<uint32_t>(frequency_steps) >= 0.5) {
      return static_cast<uint32_t>(frequency_steps) + 1;
    } else {
      return static_cast<uint32_t>(frequency_steps);
    }
  }();

  // frequency is represented as a 24-bit value, so it must be less than 2^24.
  if (frequency_steps_rounded > (1 << 24) - 1) {
    return (1 << 24) - 1;
  };

  return frequency_steps_rounded;
}

enum class OpModes : uint8_t {
  SLEEP = 0b000,
  STANDBY = 0b001,
  FrequencySynthesisTX = 0b010,
  Transmit = 0b011,
  FrequencySynthesisRX = 0b100,
  ReceiveContinuous = 0b101,
  ReceiveSingle = 0b110,
  ChannelActivityDetection = 0b111,
};

/// @brief Print the string representation of OpModes using printf
/// @param mode The OpModes value to print
inline void print_op_mode(OpModes mode) {
  switch (mode) {
  case OpModes::SLEEP:
    printf("OpMode: SLEEP\n");
    break;
  case OpModes::STANDBY:
    printf("OpMode: STANDBY\n");
    break;
  case OpModes::FrequencySynthesisTX:
    printf("OpMode: Frequency Synthesis TX\n");
    break;
  case OpModes::Transmit:
    printf("OpMode: Transmit\n");
    break;
  case OpModes::FrequencySynthesisRX:
    printf("OpMode: Frequency Synthesis RX\n");
    break;
  case OpModes::ReceiveContinuous:
    printf("OpMode: Receive Continuous\n");
    break;
  case OpModes::ReceiveSingle:
    printf("OpMode: Receive Single\n");
    break;
  case OpModes::ChannelActivityDetection:
    printf("OpMode: Channel Activity Detection\n");
    break;
  default:
    printf("OpMode: Unknown\n");
    break;
  }
}

/// @brief Spreading factors for LoRa modulation.
/// These values represent the spreading factor used in LoRa modulation.
enum class SpreadingFactors : uint8_t {
  SF6 = 6,
  SF7 = 7,
  SF8 = 8,
  SF9 = 9,
  SF10 = 10,
  SF11 = 11,
  SF12 = 12,
};

/// @brief Coding Rate for LoRa cyclic error coding.
enum class CodingRate : uint8_t {
  CR4_5 = 1,
  CR4_6 = 2,
  CR4_7 = 3,
  CR4_8 = 4,
};

/// @brief Signal bandwidths for LoRa modulation.
/// The enum values correspond to the 4-bit register values for bandwidth
/// selection.
enum class SignalBandwidth : uint8_t {
  BW_7_8_kHz = 0b0000,
  BW_10_4_kHz = 0b0001,
  BW_15_6_kHz = 0b0010,
  BW_20_8_kHz = 0b0011,
  BW_31_25_kHz = 0b0100,
  BW_41_7_kHz = 0b0101,
  BW_62_5_kHz = 0b0110,
  BW_125_kHz = 0b0111,
  BW_250_kHz = 0b1000,
  BW_500_kHz = 0b1001,
};

enum class ImplicitHeaderMode : uint8_t {
  Explicit = 0,
  Implicit = 1,
};

enum class TxContinuousMode : uint8_t {
  Normal = 0,
  Continuous = 1,
};

enum class RxPayloadCrcOn : uint8_t {
  Off = 0,
  On = 1,
};

/// @brief Long Range Mode selection for RegOpMode
enum class LongRangeMode : uint8_t {
  FSK_OOK = 0,
  LoRa = 1,
};

constexpr register_t RegFifo = 0x00;
// constexpr register_t RegOpMode = 0x01;
constexpr register_t RegFrMsb = 0x06;
constexpr register_t RegFrMid = 0x07;
constexpr register_t RegFrLsb = 0x08;

constexpr register_t RegFifoAddrPtr = 0x0D;
constexpr register_t RegFifoTxBaseAddr = 0x0E;
constexpr register_t RegFifoRxBaseAddr = 0x0F;
constexpr register_t RegFifoRxCurrentAddr = 0x10;
// constexpr register_t RegIrqFlags = 0x10;
constexpr register_t RegIrqFlagsMask = 0x11;

constexpr register_t RegPayloadLength = 0x22;

// @brief Register containing FifoRxBytesNb
constexpr register_t RegRxNbBytes = 0x13;

struct RegOpMode {
  static constexpr register_t addr = 0x01;

  /// @brief Set bit 7 to LongRangeMode (0: FSK/OOK, 1: LoRa)
  static inline uint8_t set_long_range_mode(uint8_t value, LongRangeMode mode) {
    return (value & 0b01111111) | (std::to_underlying(mode) << 7);
  }

  /// @brief Set bits 2-0 to Mode (device operating mode)
  static inline uint8_t set_mode(uint8_t value, OpModes mode) {
    return (value & 0b11111000) | std::to_underlying(mode);
  }
};

struct RegModemConfig2 {
  static constexpr register_t addr = 0x1E;

  // Set bits 7-4 to the spreading factor value
  static inline uint8_t set_spreading_factor(uint8_t value,
                                             SpreadingFactors sf) {
    return (value & 0b00001111) | (std::to_underlying(sf) << 4);
  }

  // Set bit 3 to TxContinuousMode (0: normal, 1: continuous)
  static inline uint8_t set_tx_continuous_mode(uint8_t value,
                                               TxContinuousMode mode) {
    return (value & 0b11110111) | (std::to_underlying(mode) << 3);
  }

  // Set bit 2 to RxPayloadCrcOn
  static inline uint8_t set_rx_payload_crc_on(uint8_t value,
                                              RxPayloadCrcOn crc_on) {
    return (value & 0b11111011) | (std::to_underlying(crc_on) << 2);
  }

  // Set bits 1-0 to SymbTimeout (MSB bits 9:8)
  static inline uint8_t set_symb_timeout_msb(uint8_t value,
                                             uint8_t symb_timeout_msb) {
    // symb_timeout_msb should be 2 bits (bits 9:8 of timeout)
    return (value & 0b11111100) | symb_timeout_msb;
  }
};

struct RegModemConfig1 {
  static constexpr register_t addr = 0x1D;

  // Set bits 7-4 to the signal bandwidth value
  static inline uint8_t set_bandwidth(uint8_t value, SignalBandwidth bw) {
    return (value & 0b00001111) | (std::to_underlying(bw) << 4);
  }

  // Set bits 3-1 to the coding rate value
  static inline uint8_t set_coding_rate(uint8_t value, CodingRate cr) {
    return (value & 0b11110001) | (std::to_underlying(cr) << 1);
  }

  // Set bit 0 to ImplicitHeaderMode
  static inline uint8_t set_implicit_header_mode(uint8_t value,
                                                 ImplicitHeaderMode mode) {
    return (value & 0b11111110) | std::to_underlying(mode);
  }
};

struct RegIrqFlags {
  static constexpr register_t addr = 0x12;

  /// @brief Structure to hold parsed interrupt flags
  struct Flags {
    bool rx_timeout;        ///< Bit 7: Timeout interrupt
    bool rx_done;           ///< Bit 6: Packet reception complete interrupt
    bool payload_crc_error; ///< Bit 5: Payload CRC error interrupt
    bool valid_header;      ///< Bit 4: Valid header received in Rx
    bool tx_done;  ///< Bit 3: FIFO Payload transmission complete interrupt
    bool cad_done; ///< Bit 2: CAD complete
    bool fhss_change_channel; ///< Bit 1: FHSS change channel interrupt
    bool cad_detected;        ///< Bit 0: Valid Lora signal detected during CAD
                              ///< operation
  };

  /// @brief Parse the RegIrqFlags register value into individual flags
  /// @param value The raw register value
  /// @return ParsedFlags structure with individual flag states
  static inline Flags parse_flags(uint8_t value) {
    return Flags{.rx_timeout = static_cast<bool>((value >> 7) & 0b1),
                 .rx_done = static_cast<bool>((value >> 6) & 0b1),
                 .payload_crc_error = static_cast<bool>((value >> 5) & 0b1),
                 .valid_header = static_cast<bool>((value >> 4) & 0b1),
                 .tx_done = static_cast<bool>((value >> 3) & 0b1),
                 .cad_done = static_cast<bool>((value >> 2) & 0b1),
                 .fhss_change_channel = static_cast<bool>((value >> 1) & 0b1),
                 .cad_detected = static_cast<bool>((value >> 0) & 0b1)};
  }
};

} // namespace rfm95