#include "packet.h"

#include <ctime>

#include "decode3of6.h"
#include "esphome/components/wmbus_bridge_common/dll_crc.h"
#include "esphome/components/wmbus_bridge_common/link_mode.h"

#define WMBUS_PREAMBLE_SIZE (3)
#define WMBUS_MODE_C_SUFIX_LEN (2)
#define WMBUS_MODE_C_PREAMBLE (0x54)
#define WMBUS_BLOCK_A_PREAMBLE (0xCD)
#define WMBUS_BLOCK_B_PREAMBLE (0x3D)

namespace esphome {
namespace wmbus_radio {

static const char *TAG = "packet";

Packet::Packet() { this->data_.reserve(WMBUS_PREAMBLE_SIZE); }

LinkMode Packet::link_mode() {
  if (this->link_mode_ == LinkMode::UNKNOWN) {
    if (!this->data_.empty()) {
      if (this->data_[0] == WMBUS_MODE_C_PREAMBLE)
        this->link_mode_ = LinkMode::C1;
      else
        this->link_mode_ = LinkMode::T1;
    }
  }
  return this->link_mode_;
}

void Packet::set_rssi(int8_t rssi) { this->rssi_ = rssi; }

uint8_t Packet::l_field() {
  switch (this->link_mode()) {
    case LinkMode::C1:
      // In C1 we expect L-field at byte 2 (after 0x54 and block preamble)
      return this->data_.size() > 2 ? this->data_[2] : 0;

    case LinkMode::T1: {
      auto decoded = decode3of6(this->data_);
      if (decoded && !decoded->empty()) return (*decoded)[0];
      return 0;
    }

    default:
      return 0;
  }
}

size_t Packet::expected_size() {
  if (!this->expected_size_) {
    const auto l = this->l_field();
    if (l == 0) return 0;

    const auto nrBlocks = l < 26 ? 2 : (l - 26) / 16 + 3;
    const auto nrBytes  = (size_t)l + 1 + 2 * (size_t)nrBlocks;

    if (this->link_mode() != LinkMode::C1) {
      this->expected_size_ = encoded_size(nrBytes);
    } else {
      if (this->data_.size() > 1 && this->data_[1] == WMBUS_BLOCK_A_PREAMBLE)
        this->expected_size_ = WMBUS_MODE_C_SUFIX_LEN + nrBytes;
      else if (this->data_.size() > 1 && this->data_[1] == WMBUS_BLOCK_B_PREAMBLE)
        this->expected_size_ = WMBUS_MODE_C_SUFIX_LEN + 1 + l;
    }
  }
  ESP_LOGV(TAG, "expected_size: %zu", this->expected_size_);
  return this->expected_size_;
}

size_t Packet::rx_capacity() {
  auto cap = this->data_.capacity() - this->data_.size();
  this->data_.resize(this->data_.capacity());
  return cap;
}

uint8_t *Packet::rx_data_ptr() { return this->data_.data() + this->data_.size(); }

bool Packet::calculate_payload_size() {
  auto total_length = this->expected_size();
  if (total_length == 0) return false;
  this->data_.reserve(total_length);
  return true;
}

std::optional<Frame> Packet::convert_to_frame() {
  std::optional<Frame> frame = {};

  ESP_LOGD(TAG, "Have data from radio (%zu bytes)", this->data_.size());

  // 1) sanity: size must match what we calculated
  if (this->expected_size() == 0 || this->expected_size() != this->data_.size()) {
    ESP_LOGW(TAG, "Drop packet: expected %zu, got %zu", this->expected_size(), this->data_.size());
    delete this;
    return frame;
  }

// 2) capture RAW bytes from radio (before any normalisation)
this->raw_data_ = this->data_;

// 3) normalize by link mode

  if (this->link_mode() == LinkMode::T1) {
    // TODO: Remove assumption that T1 is always A
    this->frame_format_ = "A";
    auto decoded_data = decode3of6(this->data_);
    if (!decoded_data) {
      ESP_LOGW(TAG, "Drop packet: 3of6 decode failed");
      delete this;
      return frame;
    }
    this->data_ = std::move(decoded_data.value());

  } else if (this->link_mode() == LinkMode::C1) {
    if (this->data_.size() > 1 && this->data_[1] == WMBUS_BLOCK_A_PREAMBLE)
      this->frame_format_ = "A";
    else if (this->data_.size() > 1 && this->data_[1] == WMBUS_BLOCK_B_PREAMBLE)
      this->frame_format_ = "B";
    else
      this->frame_format_ = "?";

    // remove 2B suffix
    this->data_.erase(this->data_.begin(), this->data_.begin() + WMBUS_MODE_C_SUFIX_LEN);

  } else {
    ESP_LOGW(TAG, "Drop packet: unknown link mode");
    delete this;
    return frame;
  }

  // 4) Strict: remove DLL CRCs (A or B). If CRC doesn't validate -> drop.
  const bool ok_crc = esphome::wmbus_common::removeAnyDLLCRCs(this->data_);
  if (!ok_crc) {
    ESP_LOGW(TAG, "Drop packet: DLL CRC check/trim failed");
    delete this;
    return frame;
  }

  // 5) minimal validity check: L-field must fit
  if (this->data_.size() < 11) {
    ESP_LOGW(TAG, "Drop packet: too short after normalize (%zu)", this->data_.size());
    delete this;
    return frame;
  }

  const size_t want = (size_t)this->data_[0] + 1;
  if (want <= this->data_.size()) {
    // if there is any trailing junk, cut it
    this->data_.resize(want);
  } else {
    ESP_LOGW(TAG, "Drop packet: L-field wants %zu but have %zu", want, this->data_.size());
    delete this;
    return frame;
  }

  frame.emplace(this);
  delete this;
  return frame;
}

Frame::Frame(Packet *packet)
    : data_(std::move(packet->data_)),
      link_mode_(packet->link_mode_),
      rssi_(packet->rssi_),
      format_(packet->frame_format_),
      raw_data_(std::move(packet->raw_data_)) {}

std::vector<uint8_t> &Frame::data() { return this->data_; }
LinkMode Frame::link_mode() { return this->link_mode_; }
int8_t Frame::rssi() { return this->rssi_; }
std::string Frame::format() { return this->format_; }

std::vector<uint8_t> Frame::as_raw() { return this->data_; }
std::string Frame::as_hex() { return format_hex(this->data_); }

std::string Frame::as_hex_raw() { return format_hex(this->raw_data_); }

std::string Frame::as_rtlwmbus() {
  const size_t time_repr_size = sizeof("YYYY-MM-DD HH:MM:SS.00Z");
  char time_buffer[time_repr_size];
  auto t = std::time(NULL);
  std::strftime(time_buffer, time_repr_size, "%F %T.00Z", std::gmtime(&t));

  auto output = std::string{};
  output.reserve(2 + 5 + 24 + 1 + 4 + 5 + 2 * this->data_.size() + 1);

  output += esphome::wmbus_common::linkModeName(this->link_mode_);
  output += ";1;1;";
  output += time_buffer;
  output += ';';
  output += std::to_string(this->rssi_);
  output += ";;;0x";
  output += this->as_hex();
  output += "\n";
  return output;
}

void Frame::mark_as_handled() { this->handlers_count_++; }
uint8_t Frame::handlers_count() { return this->handlers_count_; }

} // namespace wmbus_radio
} // namespace esphome
