#include "fseq_v2.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <fstream>
#include <system_error>

namespace VLT {

//
// Helpers
//

/// Reads the contents of the file as a vector<byte>.
/// @param p Path to file.
/// @return std::vector<byte> filled with the file contents.
/// @throw std::filesystem::filesystem_error
static std::vector<std::byte> read_file_contents(const std::filesystem::path& p) {
  try {
    std::vector<std::byte> contents(std::filesystem::file_size(p));
    std::ifstream file{};
    file.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    file.open(p, std::ios::binary);
    file.read(reinterpret_cast<char*>(contents.data()), static_cast<std::streamsize>(contents.size()));
    return contents;
  } catch (const std::ios_base::failure& e) {
    throw std::filesystem::filesystem_error{"cannot read file contents", p, e.code()};
  }
}

static void write_file_contents(const std::filesystem::path& p, const std::vector<std::byte>& contents) {
  try {
    std::ofstream file{};
    file.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    file.open(p, std::ios::binary);
    file.write(reinterpret_cast<const char*>(contents.data()), contents.size());
  } catch (const std::ios_base::failure& e) {
    throw std::filesystem::filesystem_error{"cannot write file contents", p, e.code()};
  }
}

template <std::integral T>
static constexpr T le_to_native(T input) {
  if constexpr (std::endian::native == std::endian::little) return input;
  return std::byteswap(input);
}

template <std::integral T>
static constexpr T native_to_le(T input) {
  return le_to_native(input);
}

//
// End helpers
//

//
// FSEQv2_Variable
//
struct FSEQv2_Variable {
  using size_type = uint16_t;
  static constexpr std::size_t CODE_LENGTH{2};
  static constexpr std::size_t HEADER_LENGTH{sizeof(size_type) + CODE_LENGTH};
  static constexpr std::size_t MAX_DATA_LENGTH{std::numeric_limits<size_type>::max() - HEADER_LENGTH};
  size_type size{};
  std::string code;
  std::string data;
};
static FSEQv2_Variable parse_fseq_variable(std::span<const std::byte> raw) {
  FSEQv2_Variable var;

  if (raw.size() < (sizeof(FSEQv2_Variable::size_type) + var.CODE_LENGTH)) return var;

  var.size = le_to_native(*(reinterpret_cast<const FSEQv2_Variable::size_type*>(raw.data())));
  raw = raw.subspan(sizeof(FSEQv2_Variable::size_type));

  if (var.size == 0) return var;

  var.code = std::string(reinterpret_cast<const char*>(raw.data()), FSEQv2_Variable::CODE_LENGTH);
  raw = raw.subspan(FSEQv2_Variable::CODE_LENGTH);

  auto data_size = var.size - 4;
  if (raw.size() < data_size) {
    throw std::runtime_error{"variable size overrun"};
  }
  var.data = std::string(reinterpret_cast<const char*>(raw.data()), data_size);

  return var;
}
static std::vector<std::byte> serialize_fseq_variable(std::string code, const std::string& data) {
  if (code.size() != FSEQv2_Variable::CODE_LENGTH) {
    throw std::invalid_argument{"serialize_fseq_variable: invalid code length"};
  }
  if (data.size() > FSEQv2_Variable::MAX_DATA_LENGTH) {
    throw std::out_of_range{"serialize_fseq_variable: data too long"};
  }
  auto size = static_cast<FSEQv2_Variable::size_type>(FSEQv2_Variable::HEADER_LENGTH + data.size());
  auto size_le = native_to_le(size);
  auto size_as_bytes = std::span<const std::byte, sizeof(FSEQv2_Variable::size_type)>{
      reinterpret_cast<const std::byte*>(&size_le),
      sizeof(FSEQv2_Variable::size_type),
  };
  auto code_as_bytes = std::span<const std::byte, FSEQv2_Variable::CODE_LENGTH>{
      reinterpret_cast<const std::byte*>(code.data()),
      code.size(),
  };
  auto data_as_bytes = std::span<const std::byte>{
      reinterpret_cast<const std::byte*>(data.data()),
      data.size(),
  };
  std::vector<std::byte> serialized;
  serialized.reserve(size_as_bytes.size() + code_as_bytes.size() + data_as_bytes.size());
  serialized.append_range(size_as_bytes);
  serialized.append_range(code_as_bytes);
  serialized.append_range(data_as_bytes);
  return serialized;
}
//
// End FSEQv2_Variable
//

//
// FSEQv2_Header
//
__pragma(pack(push, 1));
struct FSEQv2_Header {
  std::array<char, 4> identifier{'P', 'S', 'E', 'Q'};
  uint16_t ch_data_offset{0};
  uint8_t version_minor{0};
  uint8_t version_major{2};
  uint16_t var_data_offset{0};
  uint32_t channel_count{0};
  uint32_t frame_count{0};
  uint8_t step_time{0};
  uint8_t flags{0};
  uint8_t compression_block_count_upper_bits : 4 {0};
  uint8_t compression_type : 4 {0};
  uint8_t compression_block_count_lower_bits{0};
  uint8_t sparse_range_count{0};
  uint8_t _reserved_{0};
  uint64_t timestamp_us{0};

  FSEQv2_Header() = default;

  FSEQv2_Header(std::span<const std::byte> raw) {
    try {
      if (raw.size() != sizeof(*this)) throw std::runtime_error{"Invalid size"};
      std::memcpy(this, raw.data(), sizeof(*this));
      if (std::memcmp(identifier.data(), "PSEQ", 4) != 0) throw std::runtime_error{"Invalid magic"};
      if (version_major != 2) throw std::runtime_error{"Invalid major version; expected 2"};
      if (flags != 0) throw std::runtime_error{"Non-zero flags field not supported"};
      if (compression_type != 0) throw std::runtime_error{"Compression not supported"};
      if (compression_block_count_lower_bits != 0 || compression_block_count_upper_bits != 0) {
        throw std::runtime_error{"Compression blocks present; unsupported"};
      }
      if (sparse_range_count != 0) throw std::runtime_error{"Sparse channel ranges not supported"};
    } catch (const std::runtime_error& e) {
      throw std::runtime_error{std::string{"FSEQv2_Header: "} + e.what()};
    }
  }
};
__pragma(pack(pop));

static_assert(sizeof(FSEQv2_Header) == 32, "FSEQv2_Header size mismatch");
//
// End FSEQv2_Header
//

//
// FSEQv2
//
FSEQv2::FSEQv2(uint32_t num_channels, std::chrono::milliseconds step_time)
    : num_channels_{num_channels}, step_time_{step_time} {
  if (step_time_.count() > std::numeric_limits<uint8_t>::max()) {
    throw std::invalid_argument{"FSEQv2: too long step time"};
  }
}
FSEQv2::FSEQv2(const std::filesystem::path& p) { parse_from_(read_file_contents(p)); }
FSEQv2::FSEQv2(std::span<const std::byte> contents) { parse_from_(contents); }

std::vector<std::byte> FSEQv2::serialize() const {
  // First, serialize the variables
  std::vector<std::byte> variable_data_block;
  for (const auto& [variable_code, variable_data] : variables_) {
    variable_data_block.append_range(serialize_fseq_variable(variable_code, variable_data));
  }
  // Possibly extend the variable data block to a multiple of uint32_t size
  while ((variable_data_block.size() % sizeof(uint32_t)) != 0) {
    variable_data_block.push_back(std::byte{});
  }
  // Check that the variable data block isn't too large (ch_data_offset must fit into uint16_t)
  if ((variable_data_block.size() + sizeof(FSEQv2_Header)) > std::numeric_limits<uint16_t>::max()) {
    throw std::runtime_error{"variable data too long"};
  }

  // Then, populate the header
  FSEQv2_Header header;
  header.version_minor = version_minor_;
  header.var_data_offset = sizeof(FSEQv2_Header);
  header.ch_data_offset = static_cast<uint16_t>(header.var_data_offset + variable_data_block.size());
  header.channel_count = num_channels_;
  header.frame_count = num_frames_;
  header.step_time = static_cast<uint8_t>(step_time_.count());
  header.timestamp_us = created_.time_since_epoch().count();

  auto header_as_bytes = std::span<const std::byte, sizeof(FSEQv2_Header)>{
      reinterpret_cast<const std::byte*>(&header),
      sizeof(FSEQv2_Header),
  };

  std::vector<std::byte> serialized;
  serialized.reserve(header_as_bytes.size() + variable_data_block.size() + frame_data_.size());
  serialized.append_range(header_as_bytes);
  serialized.append_range(variable_data_block);
  serialized.append_range(frame_data_);
  return serialized;
}

void FSEQv2::serialize(const std::filesystem::path& output_file) const {
  write_file_contents(output_file, serialize());
}

std::chrono::milliseconds FSEQv2::total_duration() const { return (step_time_ * num_frames_); }

FSEQv2& FSEQv2::add_variable(std::string code, const std::string& value) {
  if (code.size() != FSEQv2_Variable::CODE_LENGTH) throw std::invalid_argument{"Invalid code length"};
  variables_[code] = value;
  return *this;
}

std::optional<FSEQv2::Frame> FSEQv2::frame(std::size_t idx) const {
  if (idx >= num_frames_) return {};
  return Frame{*this, idx};
}

FSEQv2& FSEQv2::reserve_frames(std::size_t num_frames) {
  frame_data_.reserve(num_frames * num_channels_);
  return *this;
}
FSEQv2& FSEQv2::add_frame(const std::vector<std::byte>& frame_data) {
  if (frame_data.size() != num_channels_) {
    throw std::invalid_argument{"FSEQv2::add_frame: invalid channel count"};
  }
  num_frames_++;
  frame_data_.append_range(frame_data);
  return *this;
}

void FSEQv2::parse_from_(std::span<const std::byte> contents) {
  FSEQv2_Header header{contents.first(sizeof(FSEQv2_Header))};

  // Save required data for later use
  version_minor_ = header.version_minor;
  num_channels_ = le_to_native(header.channel_count);
  num_frames_ = le_to_native(header.frame_count);
  step_time_ = std::chrono::milliseconds{le_to_native(header.step_time)};
  created_ = FSEQv2::time_point{std::chrono::microseconds{le_to_native(header.timestamp_us)}};

  // Process variables
  auto variable_data_length = header.ch_data_offset - header.var_data_offset;
  auto variable_data = contents.subspan(header.var_data_offset, variable_data_length);
  while (!variable_data.empty()) {
    auto var = parse_fseq_variable(variable_data);
    if (var.size == 0) break;
    variables_[var.code] = var.data;
    variable_data = variable_data.subspan(var.size);
  }

  frame_data_.assign_range(contents.subspan(header.ch_data_offset));
  if (frame_data_.size() != (num_channels_ * num_frames_)) {
    throw std::runtime_error{"channel data block size wrong"};
  }
}
//
// End FSEQv2
//

//
// FSEQv2::Frame
//
FSEQv2::Frame::Frame(const FSEQv2& seq, std::size_t idx) : seq_{&seq}, idx_{idx} {}
std::chrono::milliseconds FSEQv2::Frame::offset() const { return seq_->step_time_ * idx_; }
std::byte FSEQv2::Frame::channel_data(std::size_t ch_idx) const {
  auto frame_data_offset = (idx_ * seq_->num_channels_) + ch_idx;
  return seq_->frame_data_.at((idx_ * seq_->num_channels_) + ch_idx);
}
std::optional<FSEQv2::Frame> FSEQv2::Frame::next() const { return seq_->frame(idx_ + 1); }
std::string FSEQv2::Frame::dump(std::size_t n_chans, const Frame* previous) const {
  std::string ch_data;
  bool truncated{false};
  for (std::size_t ch = 0; ch < seq_->num_channels_; ch++) {
    if (n_chans && ch >= n_chans) {
      truncated = true;
      break;
    }
    if (previous) {
      if (channel_data(ch) == previous->channel_data(ch)) {
        ch_data += "   ";
        continue;
      }
    }
    ch_data += std::format(" {:2x}", static_cast<uint8_t>(channel_data(ch)));
  }
  if (std::ranges::all_of(ch_data, [](char c) { return c == ' '; })) return "";
  return std::format("{:>9} [{}{}]\n", offset(), ch_data, truncated ? " ..." : "");
}
//
// End FSEQv2::Frame
//

}  // namespace VLT