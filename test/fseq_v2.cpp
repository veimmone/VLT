#include "fseq_v2.h"

#include <catch2/catch_all.hpp>

#include "utils/endian.h"
#include "utils/pack.h"

TEST_CASE("FSEQv2 parsing & serializing") {
  // clang-format off
  using namespace VLT::TestUtils::Pack;
  using VLT::TestUtils::convert_to;

  constexpr auto FSEQ_endian{std::endian::little};

  constexpr auto var_mf = VLT_PACK_BYTES(
    as(convert_to<FSEQ_endian, uint16_t>(20)),
    str("mf"),
    str("deadbeefcafe.wav")
  );

  constexpr auto var_sp = VLT_PACK_BYTES(
    as(convert_to<FSEQ_endian, uint16_t>(22)),
    str("sp"),
    str("VLT creator v0.0.7")
  );

  constexpr size_t hdr_size{32};
  constexpr size_t var_size{sizeof(var_mf) + sizeof(var_sp)};
  constexpr std::array<std::byte, (var_size % 4)> var_padding{std::byte{0}};

  constexpr size_t var_offset{hdr_size};
  constexpr size_t data_offset{var_offset + var_size + sizeof(var_padding)};

  constexpr auto frames = VLT_PACK_BYTES(
    VLT_FROM_HEX("00010203"),
    VLT_FROM_HEX("04050607"),
    VLT_FROM_HEX("08090a0b"),
    VLT_FROM_HEX("0c0d0e0f")
  );

  constexpr uint64_t timestamp_us{1742822121000000};

  constexpr auto dummy_show = VLT_PACK_BYTES(
      str("PSEQ"),
      as(convert_to<FSEQ_endian, uint16_t>(data_offset)),
      as(convert_to<FSEQ_endian, uint8_t>(0)),  // version_minor
      as(convert_to<FSEQ_endian, uint8_t>(2)),  // version_major
      as(convert_to<FSEQ_endian, uint16_t>(var_offset)),
      as(convert_to<FSEQ_endian, uint32_t>(4)), // channel_count
      as(convert_to<FSEQ_endian, uint32_t>(4)), // frame_count
      as(convert_to<FSEQ_endian, uint8_t>(20)), // step_time
      as(convert_to<FSEQ_endian, uint8_t>(0)),  // flags
      as(convert_to<FSEQ_endian, uint8_t>(0)),  // compression data,
      as(convert_to<FSEQ_endian, uint8_t>(0)),  // compression data
      as(convert_to<FSEQ_endian, uint8_t>(0)),  // sparse_range_count,
      as(convert_to<FSEQ_endian, uint8_t>(0)),  // reserved
      as(convert_to<FSEQ_endian, uint64_t>(timestamp_us)),
      var_mf,
      var_sp,
      var_padding,
      frames
  );

  static_assert(
    sizeof(dummy_show) == (hdr_size + var_size + sizeof(var_padding) + sizeof(frames)),
    "wrong size!"
  );
  // clang-format on

  std::optional<VLT::FSEQv2> dummy;
  REQUIRE_NOTHROW(dummy.emplace(dummy_show));

  REQUIRE(dummy->created().time_since_epoch().count() == timestamp_us);
  REQUIRE(dummy->num_channels() == 4);
  REQUIRE(dummy->num_frames() == 4);
  REQUIRE(dummy->step_duration() == std::chrono::milliseconds{20});
  REQUIRE(dummy->total_duration() == std::chrono::milliseconds{80});
  REQUIRE(dummy->variables().size() == 2);
  REQUIRE_NOTHROW(dummy->variables().at("mf"));
  REQUIRE_NOTHROW(dummy->variables().at("sp"));
  REQUIRE(dummy->variables().at("mf") == "deadbeefcafe.wav");
  REQUIRE(dummy->variables().at("sp") == "VLT creator v0.0.7");

  auto frame = dummy->frame();

  REQUIRE(frame.has_value() == true);
  REQUIRE(frame->offset() == std::chrono::milliseconds{0});
  REQUIRE(frame->channel_data(0) == std::byte{0x00});
  REQUIRE(frame->channel_data(1) == std::byte{0x01});
  REQUIRE(frame->channel_data(2) == std::byte{0x02});
  REQUIRE(frame->channel_data(3) == std::byte{0x03});

  frame = frame->next();

  REQUIRE(frame.has_value() == true);
  REQUIRE(frame->offset() == std::chrono::milliseconds{20});
  REQUIRE(frame->channel_data(0) == std::byte{0x04});
  REQUIRE(frame->channel_data(1) == std::byte{0x05});
  REQUIRE(frame->channel_data(2) == std::byte{0x06});
  REQUIRE(frame->channel_data(3) == std::byte{0x07});

  frame = frame->next();

  REQUIRE(frame.has_value() == true);
  REQUIRE(frame->offset() == std::chrono::milliseconds{40});
  REQUIRE(frame->channel_data(0) == std::byte{0x08});
  REQUIRE(frame->channel_data(1) == std::byte{0x09});
  REQUIRE(frame->channel_data(2) == std::byte{0x0a});
  REQUIRE(frame->channel_data(3) == std::byte{0x0b});

  frame = frame->next();

  REQUIRE(frame.has_value() == true);
  REQUIRE(frame->offset() == std::chrono::milliseconds{60});
  REQUIRE(frame->channel_data(0) == std::byte{0x0c});
  REQUIRE(frame->channel_data(1) == std::byte{0x0d});
  REQUIRE(frame->channel_data(2) == std::byte{0x0e});
  REQUIRE(frame->channel_data(3) == std::byte{0x0f});

  frame = frame->next();

  REQUIRE(frame.has_value() == false);

  REQUIRE(std::ranges::equal(dummy->serialize(), dummy_show) == true);
}