#pragma once

#include <bit>
#include <concepts>

namespace VLT::TestUtils {

template <std::endian E, std::integral T>
constexpr T convert_from(T input) {
  if constexpr (std::endian::native == E) return input;
  return std::byteswap(input);
}

template <std::endian E, std::integral T>
constexpr T convert_to(T input) {
  return convert_from<E>(input);
}

template <std::endian E>
constexpr float convert_from(float input) {
  return std::bit_cast<float>(convert_from<E>(std::bit_cast<uint32_t>(input)));
}

template <std::endian E>
constexpr float convert_to(float input) {
  return convert_from<E>(input);
}

template <std::endian E>
constexpr double convert_from(double input) {
  return std::bit_cast<double>(convert_from<E>(std::bit_cast<uint64_t>(input)));
}

template <std::endian E>
constexpr double convert_to(double input) {
  return convert_from<E>(input);
}

}  // namespace VLT::TestUtils