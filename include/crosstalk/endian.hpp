// The MIT License (MIT)
//
// Copyright (c) 2025 Stefan Fabian
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef CROSSTALK_ENDIAN_HPP
#define CROSSTALK_ENDIAN_HPP

#include <cstdint>
#if defined( __cpp_lib_endian ) && __cpp_lib_endian >= 201907L
  #include <bit>
#endif

namespace crosstalk
{
#if defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ ||                         \
    defined( __BIG_ENDIAN__ ) || defined( __ARMEB__ ) || defined( __THUMBEB__ ) ||                 \
    defined( __AARCH64EB__ ) || defined( _MIBSEB ) || defined( __MIBSEB ) || defined( __MIBSEB__ )
constexpr bool is_little_endian = false; // Big-endian
#elif defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ ||                    \
    defined( __LITTLE_ENDIAN__ ) || defined( __ARMEL__ ) || defined( __THUMBEL__ ) ||              \
    defined( __AARCH64EL__ ) || defined( _MIPSEL ) || defined( __MIPSEL ) || defined( __MIPSEL__ )
constexpr bool is_little_endian = true; // Little-endian
#elif defined( __cpp_lib_endian ) && __cpp_lib_endian >= 201907L
constexpr bool is_little_endian = std::endian::native == std::endian::little;
#else
inline const bool is_little_endian = []() { return ( *(const uint16_t *)"\x01\x02" == 0x0201 ); }();
#endif

#if defined( __cpp_lib_byteswap ) && __cpp_lib_byteswap >= 202110L
constexpr uint16_t byteswap( uint16_t value ) { return std::byteswap( value ); }

constexpr uint32_t byteswap( uint32_t value ) { return std::byteswap( value ); }

constexpr uint64_t byteswap( uint64_t value ) { return std::byteswap( value ); }
#else
constexpr uint16_t byteswap( uint16_t value )
{
  return static_cast<uint16_t>( value << 8 ) | static_cast<uint16_t>( value >> 8 );
}

constexpr uint32_t byteswap( uint32_t value )
{
  return ( ( value & 0x000000FF ) << 24 ) | ( ( value & 0x0000FF00 ) << 8 ) |
         ( ( value & 0x00FF0000 ) >> 8 ) | ( ( value & 0xFF000000 ) >> 24 );
}

constexpr uint64_t byteswap( uint64_t value )
{
  return ( ( value & 0x00000000000000FFuLL ) << 56 ) | ( ( value & 0x000000000000FF00uLL ) << 40 ) |
         ( ( value & 0x0000000000FF0000uLL ) << 24 ) | ( ( value & 0x00000000FF000000uLL ) << 8 ) |
         ( ( value & 0x000000FF00000000uLL ) >> 8 ) | ( ( value & 0x0000FF0000000000uLL ) >> 24 ) |
         ( ( value & 0x00FF000000000000uLL ) >> 40 ) | ( ( value & 0xFF00000000000000uLL ) >> 56 );
}
#endif

constexpr uint16_t hosttole16( uint16_t value )
{
  return is_little_endian ? value : byteswap( value );
}

constexpr uint32_t hosttole32( uint32_t value )
{
  return is_little_endian ? value : byteswap( value );
}

constexpr uint64_t hosttole64( uint64_t value )
{
  return is_little_endian ? value : byteswap( value );
}

constexpr uint16_t le16tohost( uint16_t value )
{
  return is_little_endian ? value : byteswap( value );
}

constexpr uint32_t le32tohost( uint32_t value )
{
  return is_little_endian ? value : byteswap( value );
}

constexpr uint64_t le64tohost( uint64_t value )
{
  return is_little_endian ? value : byteswap( value );
}

} // namespace crosstalk

#endif
