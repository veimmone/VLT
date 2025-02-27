#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace VLT {

class FSEQv2 {
 public:
  /// Create FSEQv2 data from scratch
  /// @param num_channels The number of channels used
  FSEQv2(uint32_t num_channels, std::chrono::milliseconds step_time);

  /// Read FSEQv2 data from a file
  /// @throw In addition to what the byte buffer overload might throw: TODO:
  /// list possible exceptions from reading a file
  FSEQv2(const std::filesystem::path&);

  /// Read FSEQv2 data from a byte buffer
  /// @throw TODO: list possible excptions from FSEQ intepretation
  FSEQv2(std::span<const std::byte>);

  std::vector<std::byte> serialize() const;
  void serialize(const std::filesystem::path&) const;

  using clock = std::chrono::system_clock;
  using time_point = std::chrono::time_point<clock, std::chrono::microseconds>;
  time_point created() const { return created_; }

  uint32_t num_channels() const { return num_channels_; }
  uint32_t num_frames() const { return num_frames_; }

  std::chrono::milliseconds step_duration() const { return step_time_; }
  std::chrono::milliseconds total_duration() const;

  const std::map<std::string, std::string>& variables() const { return variables_; }
  FSEQv2& add_variable(std::string code, const std::string& value);

  class Frame {
   public:
    std::chrono::milliseconds offset() const;
    std::byte channel_data(std::size_t channel_index) const;
    std::optional<Frame> next() const;
    std::string dump(std::size_t n_first_channels = 0, const Frame* previous = nullptr) const;

   private:
    friend class FSEQv2;
    Frame(const FSEQv2&, std::size_t idx);
    const FSEQv2* seq_;
    std::size_t idx_;
  };

  std::optional<Frame> frame(std::size_t index = 0) const;
  FSEQv2& reserve_frames(std::size_t num_frames);
  FSEQv2& add_frame(const std::vector<std::byte>& frame_data);

 private:
  friend class Frame;
  void parse_from_(std::span<const std::byte>);
  uint8_t version_minor_{};  // For round-trip codec correctness
  uint32_t num_channels_{};
  uint32_t num_frames_{};
  std::chrono::milliseconds step_time_;
  time_point created_{std::chrono::time_point_cast<std::chrono::microseconds>(clock::now())};

  std::map<std::string, std::string> variables_;
  std::vector<std::byte> frame_data_;
};

}  // namespace VLT