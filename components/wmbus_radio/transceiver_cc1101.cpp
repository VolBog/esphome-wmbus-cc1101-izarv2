#include "transceiver_cc1101.h"

#include <cstring>
#include <algorithm>

#include "esphome/core/log.h"

namespace esphome {
namespace wmbus_radio {

static const char *TAG = "CC1101";

void CC1101::setup() {
  this->dio1_pin_->setup();
  this->spi_setup();

  ESP_LOGV(TAG, "Setting up CC1101");

  // CC1101 has no hardware reset pin — use software reset (SRES)
  ESP_LOGVV(TAG, "software reset (SRES)");
  this->strobe(CC1101_SRES);
  delay(10);

  ESP_LOGVV(TAG, "checking part number");
  uint8_t partnum = this->read_status_register(CC1101_PARTNUM);
  uint8_t version = this->read_status_register(CC1101_VERSION);
  ESP_LOGVV(TAG, "part: %02X, version: %02X", partnum, version);
  if (partnum != 0x00) {
    ESP_LOGE(TAG, "Invalid part number: %02X (expected 0x00)", partnum);
    this->mark_failed();
    return;
  }

  ESP_LOGVV(TAG, "configuring GDO pins");
  // GDO2: Sync word sent/received (active low)
  this->write_register(CC1101_IOCFG2, CC1101_GDO_SYNC_WORD | 0x40);
  // GDO1: High impedance (unused)
  this->write_register(CC1101_IOCFG1, CC1101_GDO_HI_Z);
  // GDO0: RXFIFO threshold reached (IRQ pin), active low
  this->write_register(CC1101_IOCFG0, CC1101_GDO_RXFIFO_THR | 0x40);

  ESP_LOGVV(TAG, "configuring FIFO threshold");
  // FIFO threshold: 1 byte (GDO0 fires when any byte is in the FIFO)
  this->write_register(CC1101_FIFOTHR, 0x01);

  ESP_LOGVV(TAG, "configuring sync word");
  this->write_register(CC1101_SYNC1, WMBUS_SYNC_HI);
  this->write_register(CC1101_SYNC0, WMBUS_SYNC_LO);

  ESP_LOGVV(TAG, "configuring packet control");
  this->write_register(CC1101_PKTLEN, 0xFF);
  this->write_register(CC1101_PKTCTRL1, 0x00);
  this->write_register(CC1101_PKTCTRL0, 0x02);

  this->write_register(CC1101_ADDR, 0x00);
  this->write_register(CC1101_CHANNR, 0x00);

  ESP_LOGVV(TAG, "configuring frequency synthesizer");
  this->write_register(CC1101_FSCTRL1, 0x08);
  this->write_register(CC1101_FSCTRL0, 0x00);

  ESP_LOGVV(TAG, "setting radio frequency");
  uint32_t freq_reg = ((uint64_t) this->frequency_hz_ << 16) / F_XTAL;
  this->write_register(CC1101_FREQ2, byte(freq_reg, 2));
  this->write_register(CC1101_FREQ1, byte(freq_reg, 1));
  this->write_register(CC1101_FREQ0, byte(freq_reg, 0));

  ESP_LOGVV(TAG, "configuring modem");
  // 100 kbps, 2-FSK, 50 kHz deviation
  this->write_register(CC1101_MDMCFG4, 0x5C);
  this->write_register(CC1101_MDMCFG3, 0x04);
  this->write_register(CC1101_MDMCFG2, 0x06);
  this->write_register(CC1101_MDMCFG1, 0x22);
  this->write_register(CC1101_MDMCFG0, 0xF8);

  this->write_register(CC1101_DEVIATN, 0x44);

  ESP_LOGVV(TAG, "configuring state machine");
  this->write_register(CC1101_MCSM2, 0x07);
  this->write_register(CC1101_MCSM1, 0x00);
  this->write_register(CC1101_MCSM0, 0x18);

  ESP_LOGVV(TAG, "configuring AFC/AGC");
  this->write_register(CC1101_FOCCFG, 0x2E);
  this->write_register(CC1101_BSCFG, 0xBF);
  this->write_register(CC1101_AGCCTRL2, 0x43);
  this->write_register(CC1101_AGCCTRL1, 0x09);
  this->write_register(CC1101_AGCCTRL0, 0xB5);

  ESP_LOGVV(TAG, "configuring WOR");
  this->write_register(CC1101_WOREVT1, 0x87);
  this->write_register(CC1101_WOREVT0, 0x6B);
  this->write_register(CC1101_WORCTRL, 0xFB);

  ESP_LOGVV(TAG, "configuring front end");
  this->write_register(CC1101_FREND1, 0xB6);
  this->write_register(CC1101_FREND0, 0x10);

  ESP_LOGVV(TAG, "configuring frequency calibration");
  this->write_register(CC1101_FSCAL3, 0xEA);
  this->write_register(CC1101_FSCAL2, 0x2A);
  this->write_register(CC1101_FSCAL1, 0x00);
  this->write_register(CC1101_FSCAL0, 0x1F);

  this->write_register(CC1101_RCCTRL1, 0x41);
  this->write_register(CC1101_RCCTRL0, 0x00);

  ESP_LOGVV(TAG, "configuring test registers");
  this->write_register(CC1101_FSTEST, 0x59);
  this->write_register(CC1101_PTEST, 0x7F);
  this->write_register(CC1101_AGCTEST, 0x3F);
  this->write_register(CC1101_TEST2, 0x81);
  this->write_register(CC1101_TEST1, 0x35);
  this->write_register(CC1101_TEST0, 0x09);

  ESP_LOGVV(TAG, "calibrating");
  this->strobe(CC1101_SCAL);
  delay(1);

  ESP_LOGVV(TAG, "entering RX mode");
  this->strobe(CC1101_SFRX);
  this->strobe(CC1101_SRX);

  ESP_LOGI(TAG, "CC1101 setup done");
}

void CC1101::dump_config() {
  Transceiver::dump_config();
  ESP_LOGCONFIG(TAG, "Frequency: %.3f MHz", this->frequency_hz_ / 1e6f);
}

void CC1101::start_receiver(TaskHandle_t receiver_task) {
  this->data_ready_interrupt_ = {receiver_task, 1};
  this->dio1_pin_->attach_interrupt(&Transceiver::notify_interrupt, &this->data_ready_interrupt_,
                                    gpio::INTERRUPT_FALLING_EDGE);
}

bool CC1101::read(std::span<uint8_t> buffer, TickType_t first_byte_timeout) {
  // Wait for the first byte: GDO0 is active low when the FIFO has data.
  if (this->dio1_pin_->digital_read() && !ulTaskNotifyTake(pdTRUE, first_byte_timeout))
    return false;

  while (!buffer.empty()) {
    if (this->dio1_pin_->digital_read()) {
      if (!ulTaskNotifyTake(pdTRUE, SINGLE_BYTE_TIMEOUT))
        return false;
    }

    this->enable();
    this->transfer_byte(CC1101_RXFIFO | CC1101_READ_BURST);
    while (!buffer.empty() && !this->dio1_pin_->digital_read()) {
      buffer.front() = this->transfer_byte(0x00);
      buffer = buffer.subspan(1);
    }
    this->disable();
  }

  return true;
}

void CC1101::reset_receiver() {
  // Go to IDLE, flush FIFO, re-enter RX
  this->strobe(CC1101_SIDLE);
  delay(1);
  this->strobe(CC1101_SFRX);
  ulTaskNotifyTake(pdTRUE, 0);
  this->strobe(CC1101_SRX);
  delay(1);
}

int8_t CC1101::get_rssi() {
  uint8_t rssi_dec = this->read_status_register(CC1101_RSSI);
  int8_t rssi_offset = (this->frequency_hz_ < 600000000u) ? 76 : 74;
  int16_t rssi_dbm;
  if (rssi_dec >= 128) {
    rssi_dbm = ((int16_t) (rssi_dec - 256) / 2) - rssi_offset;
  } else {
    rssi_dbm = (rssi_dec / 2) - rssi_offset;
  }
  return (int8_t) rssi_dbm;
}

const char *CC1101::get_name() { return TAG; }

uint8_t CC1101::strobe(uint8_t cmd) {
  this->enable();
  uint8_t status = this->transfer_byte(cmd);
  this->disable();
  return status;
}

uint8_t CC1101::read_register(uint8_t address) {
  std::array<uint8_t, 2> transaction{static_cast<uint8_t>(address | CC1101_READ_SINGLE), 0x00};
  this->spi_transaction(transaction);
  return transaction[1];
}

uint8_t CC1101::read_status_register(uint8_t address) {
  std::array<uint8_t, 2> transaction{static_cast<uint8_t>(address | CC1101_READ_BURST), 0x00};
  this->spi_transaction(transaction);
  return transaction[1];
}

void CC1101::write_register(uint8_t address, uint8_t value) {
  std::array<uint8_t, 2> transaction{static_cast<uint8_t>(address | CC1101_WRITE_SINGLE), value};
  this->spi_transaction(transaction);
}

}  // namespace wmbus_radio
}  // namespace esphome
