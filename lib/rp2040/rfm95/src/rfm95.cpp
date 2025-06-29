#include "rfm95/rfm95.hpp"
#include <pico/rand.h>
#include <utility>

namespace rfm95 {

RFM95::RFM95(spi_inst_t *spi, std::size_t cs_pin,
             RecieveCallback on_recieve_callback)
    : spi_(spi, cs_pin), on_receive_callback_(on_recieve_callback) {}

OpModes RFM95::read_device_mode() const {
  const auto reg_op_mode = spi_.read_register(RegOpMode::addr);
  const OpModes mode = static_cast<OpModes>(reg_op_mode & 0b111);
  return mode;
}

RFM95 &RFM95::set_device_mode(OpModes mode) const {
  if (current_mode() == mode) {
    // no need to change mode
    return *const_cast<RFM95 *>(this);
  }

  spi_.update_register(RegOpMode::addr, [&](uint8_t value) {
    return rfm95::RegOpMode::set_mode(value, mode);
  });
  return *const_cast<RFM95 *>(this);
}

RFM95 &RFM95::set_spreading_factor(SpreadingFactors sf) const {
  spi_.update_register(RegModemConfig2::addr, [&](uint8_t value) {
    return rfm95::RegModemConfig2::set_spreading_factor(value, sf);
  });
  return *const_cast<RFM95 *>(this);
}

RFM95 &RFM95::set_bandwidth(SignalBandwidth bw) const {
  spi_.update_register(RegModemConfig1::addr, [&](uint8_t value) {
    return rfm95::RegModemConfig1::set_bandwidth(value, bw);
  });
  return *const_cast<RFM95 *>(this);
}

RFM95 &RFM95::set_coding_rate(CodingRate cr) const {
  spi_.update_register(RegModemConfig1::addr, [&](uint8_t value) {
    return rfm95::RegModemConfig1::set_coding_rate(value, cr);
  });
  return *const_cast<RFM95 *>(this);
}

RFM95 &RFM95::set_header_mode(ImplicitHeaderMode mode) const {
  spi_.update_register(RegModemConfig1::addr, [&](uint8_t value) {
    return rfm95::RegModemConfig1::set_implicit_header_mode(value, mode);
  });
  return *const_cast<RFM95 *>(this);
}

RFM95 &RFM95::set_long_range_mode(LongRangeMode mode) const {
  printf("Setting Long Range Mode\n");
  set_device_mode(OpModes::SLEEP);
  spi_.update_register(RegOpMode::addr, [&](uint8_t value) {
    return rfm95::RegOpMode::set_long_range_mode(value, mode);
  });
  set_device_mode(OpModes::STANDBY);

  return *const_cast<RFM95 *>(this);
}

RFM95 &RFM95::set_frequency(uint32_t frequency) const {
  printf("Setting Frequency\n");

  set_device_mode(OpModes::SLEEP);

  // I could replace this with one burst write, but tbh I'm not spending time
  // formatting this as a burst write for setting the frequency like once
  spi_.write_register(rfm95::RegFrMsb, (frequency >> 16) & 0xFF);
  spi_.write_register(rfm95::RegFrMid, (frequency >> 8) & 0xFF);
  spi_.write_register(rfm95::RegFrLsb, frequency & 0xFF);
  return *const_cast<RFM95 *>(this);
}

TransmitStateMachine RFM95::transmit(const std::span<const uint8_t> data) {
  static TransmitStateMachine state = TransmitStateMachine::IDLE;

  if (state == TransmitStateMachine::IDLE) {

    // 0.0 Put the device into standby mode
    // Use the new fluid interface for clarity
    set_device_mode(OpModes::STANDBY);

    spi_.write_register(rfm95::RegIrqFlagsMask, 0xFF);
    spi_.write_register(rfm95::RegIrqFlagsMask,
                        0b11110111); // clear all IRQ flags

    // 1.1 Set FifoPtrAddr to FifoTxPtrBase
    // 1.2 Read FifoTxBaseAddr register
    // 1.2 Write FifoPtrAddr register with the value from FifoTxBaseAddr

    const auto fifo_base_addr = spi_.read_register(rfm95::RegFifoTxBaseAddr);
    spi_.write_register(rfm95::RegFifoAddrPtr, fifo_base_addr);
    spi_.write_register(rfm95::RegFifo, data);

    // 2.0 Mode request Tx
    spi_.write_register(0x40, 0x40);
    set_device_mode(OpModes::Transmit);
    state = TransmitStateMachine::WAITING_FOR_TX_DONE;

  } else if (state == TransmitStateMachine::WAITING_FOR_TX_DONE) {
    // 3.0 Wait for the TxDone Interrupt
    {
      if (tx_done_signal_) {
        state = TransmitStateMachine::IDLE;
        tx_done_signal_ = false;
      }
    }
  }

  return state;
}

void RFM95::enable_continuous_recieve() {
  receive_enabled_ = true;
  set_device_mode(OpModes::ReceiveContinuous);
}

void RFM95::handle_recieve_interrupt(const RegIrqFlags::Flags &irq_flags) {
  bool success = !irq_flags.payload_crc_error;
  success &= irq_flags.valid_header;
  success &= !irq_flags.rx_timeout;

  if (success) {
    const auto bytes_received = read_fifo_buffer();
    on_receive_callback_(
        std::span<const uint8_t>(rx_fifo_buffer_.data(), bytes_received));
  }
}

size_t RFM95::read_fifo_buffer() {
  // 1.0 Read FifoNbRxBytes to find out how many bytes have been recieved thus
  // far
  const auto fifo_rx_bytes = spi_.read_register(RegRxNbBytes);

  // 2.0 Set FifoPtr Addr to FifoRxCurrentAddr. Sets the location to the last
  // packet recieved.
  const auto fifo_rx_current_addr =
      spi_.read_register(rfm95::RegFifoRxCurrentAddr);
  spi_.write_register(rfm95::RegFifoAddrPtr, fifo_rx_current_addr);

  // 3.0 The payload can then be extracted by reading FIFO RegNbRxBytes times.
  spi_.read_register_repeated(rfm95::RegFifo, fifo_rx_bytes, rx_fifo_buffer_);

  return fifo_rx_bytes;
}

void RFM95::interrupt_callback() {

  // read interrupt flags
  set_device_mode(OpModes::STANDBY);
  const auto irq_flag_bits = spi_.read_register(RegIrqFlags::addr);
  const auto irq_flags = RegIrqFlags::parse_flags(irq_flag_bits);

  printf("RFM95 Interrupt Flags: 0b%08b\n", irq_flag_bits);

  if (receive_enabled_ && irq_flags.rx_done) {
    handle_recieve_interrupt(irq_flags);
  }

  if (irq_flags.tx_done && !tx_done_signal_) {
    tx_done_signal_ = true;
  }
}

OpModes RFM95::current_mode() const { return read_device_mode(); }

} // namespace rfm95