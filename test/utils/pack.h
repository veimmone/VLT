#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

/// @brief Pack a set of values into one std::array<std::byte>. All parameters must themselves be
/// contiguous containers of std::byte. POD values can be converted to such with Pack::as<T> and strings
/// with Pack::str. A hex string can be packed using `VTL_FROM_HEX`.
// clang-format off
#define VLT_PACK_BYTES(...) VLT::TestUtils::Pack::detail::to_bytes<VLT::TestUtils::Pack::detail::total_size(__VA_ARGS__)>(__VA_ARGS__)
// clang-format on

/// @brief Pack a hex string into one std::array<std::byte>. The parameter string must be valid hex, e.g.
/// "DEADBEEFCAFE". Case insensitive.
// clang-format off
#define VLT_FROM_HEX(str) (static_cast<void>([]{static_assert(VLT::TestUtils::Pack::detail::check_hex(str), "Invalid hex string");}()), VLT::TestUtils::Pack::detail::hex(str))
// clang-format on

namespace VLT::TestUtils::Pack {

template <typename T>
concept ScalarPod = std::is_standard_layout_v<T> && std::is_trivial_v<T> && std::is_scalar_v<T>;

/// @brief Pack a singular POD value
/// @tparam T POD type to pack as
/// @param value Value to pack (will be implicitly converted to T, if conversion available)
/// @return Array of bytes representing the packed value.
template <ScalarPod T>
constexpr std::array<std::byte, sizeof(T)> as(const T& value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}

/// @brief Pack a const string
/// @param str A compile-time constant character array, e.g. "FOO"
/// @return Array of bytes representing the packed string.
template <std::size_t N>
constexpr std::array<std::byte, N - 1> str(const char (&str)[N]) {
  std::array<std::byte, N - 1> bytes{};
  for (std::size_t i = 0; i < N - 1; ++i) {
    bytes[i] = static_cast<std::byte>(str[i]);
  }
  return bytes;
}

/// @brief Implementation details used by the `VTL_PACK_BYTES` and `VTL_FROM_HEX` macros. Do not use
/// directly.
namespace detail {

constexpr bool check_hex(std::string_view hex_str) {
  if ((hex_str.size() % 2) != 0) return false;
  auto check_char = [](char c) {
    if (c >= '0' && c <= '9') return true;
    if (c >= 'a' && c <= 'f') return true;
    if (c >= 'A' && c <= 'F') return true;
    return false;
  };
  for (auto c : hex_str) {
    if (!check_char(c)) return false;
  }
  return true;
}

template <std::size_t N>
constexpr std::array<std::byte, (N - 1) / 2> hex(const char (&str)[N]) {
  static_assert(((N % 2) == 1), "Wrong size for hex string");  // Trailing null for const char*
  constexpr std::size_t n_bytes{(N - 1) / 2};
  std::array<std::byte, n_bytes> bytes{};

  auto hex_byte = [](char h, char l) -> std::byte {
    auto hex_nibble = [](char c) -> std::byte {
      if (c >= '0' && c <= '9') return static_cast<std::byte>(c - '0');
      if (c >= 'a' && c <= 'f') return static_cast<std::byte>(c - 'a' + 10);
      if (c >= 'A' && c <= 'F') return static_cast<std::byte>(c - 'A' + 10);
      return {};
    };
    return ((hex_nibble(h) << 4) | hex_nibble(l));
  };

  for (std::size_t i = 0; i < n_bytes; i++) {
    auto j = 2 * i;  // 0, 2, 4, ...
    bytes[i] = hex_byte(str[j], str[j + 1]);
  }
  return bytes;
}

template <typename... Ts>
constexpr std::size_t total_size(const Ts&... values) {
  return (0 + ... + values.size());
}

template <std::size_t total_size, typename... Ts>
static constexpr std::array<std::byte, total_size> to_bytes(const Ts&... values) {
  std::vector<std::byte> tmp;
  (tmp.append_range(values), ...);
  std::array<std::byte, total_size> packed{};
  std::copy(tmp.begin(), tmp.end(), packed.begin());
  return packed;
}

}  // namespace detail
}  // namespace VLT::TestUtils::Pack