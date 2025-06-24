#pragma once

#include <stdint.h>
#include <utility>

namespace rfm95 {
using register_t = uint8_t;

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

constexpr register_t RegFifo = 0x00;
constexpr register_t RegOpMode = 0x01;
constexpr register_t RegFrMsb = 0x06;
constexpr register_t RegFrMid = 0x07;
constexpr register_t RegFrLsb = 0x08;
constexpr register_t RegPaConfig = 0x09;
constexpr register_t RegPaRamp = 0x0A;
constexpr register_t RegOcp = 0x0B;
constexpr register_t RegLna = 0x0C;
constexpr register_t RegFifoAddrPtr = 0x0D;
constexpr register_t RegFifoTxBaseAddr = 0x0E;
constexpr register_t RegFifoRxBaseAddr = 0x0F;
constexpr register_t RegIrqFlags = 0x10;
constexpr register_t RegIrqFlagsMask = 0x11;
constexpr register_t RegFreqIfMsb = 0x12;
constexpr register_t RegFreqIFLsb = 0x13;
constexpr register_t RegSymbTimeMsb = 0x14;
constexpr register_t RegSymbTimeLsb = 0x15;
constexpr register_t RegTxCfg = 0x16;
constexpr register_t PayloadLength = 0x17;
constexpr register_t RegPreambleMsg = 0x18;
constexpr register_t RegPreambleLsb = 0x19;
constexpr register_t RegModulationCfg = 0x1A;
constexpr register_t RegRfMode = 0x1B;
constexpr register_t RegHopPeriod = 0x1C;

constexpr register_t RegNbRxBytes = 0x1D;
constexpr register_t RegRxHeaderInfo = 0x1E;
constexpr register_t RegRxHeaderCntValue = 0x1F;
constexpr register_t RegRxPacketCntValue = 0x20;
constexpr register_t RegModemStat = 0x21;
constexpr register_t RegPktSnrValue = 0x22;
constexpr register_t RegRssiValue = 0x23;
constexpr register_t RegPktRssiValue = 0x24;
constexpr register_t RegHopChannel = 0x25;
constexpr register_t RegRxDataAddr = 0x26;

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
    return (value & 0b11110111) | (static_cast<uint8_t>(mode) << 3);
  }

  // Set bit 2 to RxPayloadCrcOn
  static inline uint8_t set_rx_payload_crc_on(uint8_t value,
                                              RxPayloadCrcOn crc_on) {
    return (value & 0b11111011) | (static_cast<uint8_t>(crc_on) << 2);
  }

  // Set bits 1-0 to SymbTimeout (MSB bits 9:8)
  static inline uint8_t set_symb_timeout_msb(uint8_t value,
                                             uint8_t symb_timeout_msb) {
    // symb_timeout_msb should be 2 bits (bits 9:8 of timeout)
    return (value & 0b11111100) | (symb_timeout_msb & 0b11);
  }
};

struct RegModemConfig1 {
  static constexpr register_t addr = 0x1D;

  // Set bits 7-4 to the signal bandwidth value
  static inline uint8_t set_bandwidth(uint8_t value, SignalBandwidth bw) {
    return (value & 0b00001111) | (static_cast<uint8_t>(bw) << 4);
  }

  // Set bits 3-1 to the coding rate value
  static inline uint8_t set_coding_rate(uint8_t value, CodingRate cr) {
    return (value & 0b11110001) | (static_cast<uint8_t>(cr) << 1);
  }

  // Set bit 0 to ImplicitHeaderMode
  static inline uint8_t set_implicit_header_mode(uint8_t value,
                                                 ImplicitHeaderMode mode) {
    return (value & 0b11111110) | (static_cast<uint8_t>(mode) & 0x1);
  }
};

} // namespace rfm95