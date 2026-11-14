#pragma once

#include <cstdint>

#include "transceiver.h"

namespace esphome {
namespace wmbus_radio {

class CC1101 final : public Transceiver {
 public:
  void setup() override;
  void dump_config() override;
  void start_receiver(TaskHandle_t receiver_task) override;
  void reset_receiver() override;
  bool read(std::span<uint8_t> buffer, TickType_t first_byte_timeout) override;
  int8_t get_rssi() override;
  const char *get_name() override;

  void set_frequency_hz(uint32_t hz) { this->frequency_hz_ = hz; }

 private:
  // CC1101 SPI command strobes
  static constexpr uint8_t CC1101_SRES = 0x30;
  static constexpr uint8_t CC1101_SRX = 0x34;
  static constexpr uint8_t CC1101_SIDLE = 0x36;
  static constexpr uint8_t CC1101_SFRX = 0x3A;
  static constexpr uint8_t CC1101_SCAL = 0x33;

  // Configuration registers
  static constexpr uint8_t CC1101_IOCFG2 = 0x00;
  static constexpr uint8_t CC1101_IOCFG1 = 0x01;
  static constexpr uint8_t CC1101_IOCFG0 = 0x02;
  static constexpr uint8_t CC1101_FIFOTHR = 0x03;
  static constexpr uint8_t CC1101_SYNC1 = 0x04;
  static constexpr uint8_t CC1101_SYNC0 = 0x05;
  static constexpr uint8_t CC1101_PKTLEN = 0x06;
  static constexpr uint8_t CC1101_PKTCTRL1 = 0x07;
  static constexpr uint8_t CC1101_PKTCTRL0 = 0x08;
  static constexpr uint8_t CC1101_ADDR = 0x09;
  static constexpr uint8_t CC1101_CHANNR = 0x0A;
  static constexpr uint8_t CC1101_FSCTRL1 = 0x0B;
  static constexpr uint8_t CC1101_FSCTRL0 = 0x0C;
  static constexpr uint8_t CC1101_FREQ2 = 0x0D;
  static constexpr uint8_t CC1101_FREQ1 = 0x0E;
  static constexpr uint8_t CC1101_FREQ0 = 0x0F;
  static constexpr uint8_t CC1101_MDMCFG4 = 0x10;
  static constexpr uint8_t CC1101_MDMCFG3 = 0x11;
  static constexpr uint8_t CC1101_MDMCFG2 = 0x12;
  static constexpr uint8_t CC1101_MDMCFG1 = 0x13;
  static constexpr uint8_t CC1101_MDMCFG0 = 0x14;
  static constexpr uint8_t CC1101_DEVIATN = 0x15;
  static constexpr uint8_t CC1101_MCSM2 = 0x16;
  static constexpr uint8_t CC1101_MCSM1 = 0x17;
  static constexpr uint8_t CC1101_MCSM0 = 0x18;
  static constexpr uint8_t CC1101_FOCCFG = 0x19;
  static constexpr uint8_t CC1101_BSCFG = 0x1A;
  static constexpr uint8_t CC1101_AGCCTRL2 = 0x1B;
  static constexpr uint8_t CC1101_AGCCTRL1 = 0x1C;
  static constexpr uint8_t CC1101_AGCCTRL0 = 0x1D;
  static constexpr uint8_t CC1101_WOREVT1 = 0x1E;
  static constexpr uint8_t CC1101_WOREVT0 = 0x1F;
  static constexpr uint8_t CC1101_WORCTRL = 0x20;
  static constexpr uint8_t CC1101_FREND1 = 0x21;
  static constexpr uint8_t CC1101_FREND0 = 0x22;
  static constexpr uint8_t CC1101_FSCAL3 = 0x23;
  static constexpr uint8_t CC1101_FSCAL2 = 0x24;
  static constexpr uint8_t CC1101_FSCAL1 = 0x25;
  static constexpr uint8_t CC1101_FSCAL0 = 0x26;
  static constexpr uint8_t CC1101_RCCTRL1 = 0x27;
  static constexpr uint8_t CC1101_RCCTRL0 = 0x28;
  static constexpr uint8_t CC1101_FSTEST = 0x29;
  static constexpr uint8_t CC1101_PTEST = 0x2A;
  static constexpr uint8_t CC1101_AGCTEST = 0x2B;
  static constexpr uint8_t CC1101_TEST2 = 0x2C;
  static constexpr uint8_t CC1101_TEST1 = 0x2D;
  static constexpr uint8_t CC1101_TEST0 = 0x2E;

  // Status registers
  static constexpr uint8_t CC1101_PARTNUM = 0x30;
  static constexpr uint8_t CC1101_VERSION = 0x31;
  static constexpr uint8_t CC1101_RSSI = 0x34;
  static constexpr uint8_t CC1101_MARCSTATE = 0x35;
  static constexpr uint8_t CC1101_RXBYTES = 0x3B;

  // FIFO access
  static constexpr uint8_t CC1101_RXFIFO = 0x3F;

  // SPI access masks
  static constexpr uint8_t CC1101_WRITE_SINGLE = 0x00;
  static constexpr uint8_t CC1101_READ_SINGLE = 0x80;
  static constexpr uint8_t CC1101_READ_BURST = 0xC0;

  // GDO configuration
  static constexpr uint8_t CC1101_GDO_RXFIFO_THR = 0x00;
  static constexpr uint8_t CC1101_GDO_SYNC_WORD = 0x06;
  static constexpr uint8_t CC1101_GDO_HI_Z = 0x2E;

  // wM-Bus sync word
  static constexpr uint8_t WMBUS_SYNC_HI = 0x54;
  static constexpr uint8_t WMBUS_SYNC_LO = 0x3D;

  // Crystal frequency (26 MHz)
  static constexpr uint32_t F_XTAL = 26000000;

  uint32_t frequency_hz_{868950000};
  InterruptContext data_ready_interrupt_;
  int8_t last_rssi_{0};

  uint8_t strobe(uint8_t cmd);
  uint8_t read_register(uint8_t address);
  uint8_t read_status_register(uint8_t address);
  void write_register(uint8_t address, uint8_t value);
};

}  // namespace wmbus_radio
}  // namespace esphome
