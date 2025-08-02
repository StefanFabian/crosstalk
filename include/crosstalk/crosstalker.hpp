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

#ifndef CROSSTALK_CROSSTALKER_HPP
#define CROSSTALK_CROSSTALKER_HPP

#include "endian.hpp"
#include "refl.hpp"
#include "serial_abstraction.hpp"
#include <cassert>
#include <stddef.h>
#include <vector>

namespace crosstalk
{

/*! @brief Attribute to specify the ID of the object type for serialization.
 * This ID is used to identify the type of object being serialized/deserialized.
 * It should be unique for each type.
 * Negative id's are reserved for internal use, please do not use.
 */
struct id : public refl::attr::usage::type {
  const int16_t id_value;

  explicit constexpr id( const int16_t id ) noexcept : id_value( id ) { }
};

template<typename T>
constexpr int16_t object_id() noexcept
{
  return std::get<id>( refl::type_descriptor<T>::attributes ).id_value;
}

enum class ReadResult : uint8_t {
  Success = 0,
  NoObjectAvailable = 1,
  NotEnoughData = 2,
  CrcError = 3,
  ObjectIdMismatch = 4,
  ObjectSizeMismatch = 5, // This is usually when types without clear size are used like int or long
};

inline std::string to_string( ReadResult result )
{
  switch ( result ) {
  case ReadResult::Success:
    return "Success";
  case ReadResult::NoObjectAvailable:
    return "NoObjectAvailable";
  case ReadResult::NotEnoughData:
    return "NotEnoughData";
  case ReadResult::CrcError:
    return "CrcError";
  case ReadResult::ObjectIdMismatch:
    return "ObjectIdMismatch";
  case ReadResult::ObjectSizeMismatch:
    return "ObjectSizeMismatch";
  }
  return "UnknownReadResult";
}

enum class WriteResult : uint8_t { Success = 0, ObjectTooLarge = 1, WriteError = 2 };

inline std::string to_string( WriteResult result )
{
  switch ( result ) {
  case WriteResult::Success:
    return "Success";
  case WriteResult::ObjectTooLarge:
    return "ObjectTooLarge";
  case WriteResult::WriteError:
    return "WriteError";
  }
  return "UnknownWriteResult";
}

template<int BUFFER_SIZE = 512, int SERIALIZATION_BUFFER_SIZE = BUFFER_SIZE / 2>
class CrossTalker final
{
public:
  explicit CrossTalker( std::unique_ptr<SerialAbstraction> serial ) : serial_( std::move( serial ) )
  {
  }

  /*! Process serial data and update the internal buffer.
   *  @param overwrite_buffer If true, old data in the buffer will be overwritten if more data is
   *    available than the buffer can hold.
   */
  void processSerialData( bool overwrite_buffer = true );

  //! Returns the number of non-object bytes available in the serial buffer.
  int available() const;

  //! Returns true if the start of an object is found at the start of the serial buffer.
  bool hasObject() const;

  //! Returns the id of the available object or -1 if no object.
  int16_t getObjectId() const;

  //! Clear the internal serial buffer.
  void clearBuffer()
  {
    buffer_index_ = 0;
    buffer_size_ = 0;
  }

  //! Read non-object data from the serial buffer.
  size_t read( uint8_t *data, size_t length );

  //! Skip non-object data in the serial buffer. Only skips until the next object start marker.
  size_t skip( size_t length = BUFFER_SIZE );

  /*!
   * Reads the object from the serial buffer.
   * @return True if successful, false if object is not complete or CRC check fails.
   */
  template<typename T>
  ReadResult readObject( T &obj );

  //! Skips the current object in the buffer.
  ReadResult skipObject();

  //! Send the given object over the serial connection.
  template<typename T>
  WriteResult sendObject( const T &obj );

private:
  void _processSerialData( int max_to_read = BUFFER_SIZE );

  void _processSerialDataUntil( int index );

  uint16_t _readObjectSize( int start_index ) const;

  int _findNextObjectIndex( int start, int end ) const;

  void _markRead( int count );

  std::array<uint8_t, BUFFER_SIZE> buffer_;
  std::array<uint8_t, SERIALIZATION_BUFFER_SIZE> obj_buffer_;
  std::unique_ptr<SerialAbstraction> serial_;
  int buffer_index_ = 0;
  int buffer_size_ = 0;
};

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline void CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::_markRead( int count )
{
  buffer_size_ -= count;
  buffer_index_ += count;
  if ( buffer_index_ >= BUFFER_SIZE )
    buffer_index_ -= BUFFER_SIZE; // Wrap around the buffer index
  if ( buffer_size_ <= 0 ) {
    buffer_size_ = 0;
    buffer_index_ = 0;
  }
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline void CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::_processSerialData( int max_to_read )
{
  int available;
  while ( ( available = serial_->available() ) > 0 ) {
    if ( max_to_read == 0 )
      return;
    int index = buffer_index_ + buffer_size_;
    if ( index >= BUFFER_SIZE )
      index -= BUFFER_SIZE;
    int count = std::min( available, BUFFER_SIZE - index );
    count = std::min( count, max_to_read );
    count = serial_->read( &buffer_[index], count );
    buffer_size_ += count;
    max_to_read -= count;
    if ( buffer_size_ > BUFFER_SIZE ) {
      // Remove the oldest data to ensure buffer_size_ does not exceed BUFFER_SIZE
      _markRead( buffer_size_ - BUFFER_SIZE );
    }
  }
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline void CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::_processSerialDataUntil( int index )
{
  int max_to_read = index - buffer_index_;
  if ( max_to_read < 0 )
    max_to_read += BUFFER_SIZE;
  max_to_read += BUFFER_SIZE - buffer_size_;
  _processSerialData( max_to_read );
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline void
CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::processSerialData( bool overwrite_buffer )
{
  // Read one byte less than the buffer size to ensure we don't lose an object start marker
  if ( overwrite_buffer )
    _processSerialData( buffer_size_ == 0 ? BUFFER_SIZE : BUFFER_SIZE - 1 );
  else if ( buffer_size_ < BUFFER_SIZE )
    _processSerialData( BUFFER_SIZE - buffer_size_ );
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
uint16_t CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::_readObjectSize( int start_index ) const
{
  int index = start_index + 4; // Size is at index + 4
  if ( index >= BUFFER_SIZE )
    index -= BUFFER_SIZE;
  uint16_t serialized_size = 0;
  if ( index == BUFFER_SIZE - 1 ) {
    serialized_size = buffer_[index] | ( static_cast<uint16_t>( buffer_[0] ) << 8 );
  } else {
    std::memcpy( &serialized_size, &buffer_[index], 2 );
  }
  return le16tohost( serialized_size );
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline int CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::_findNextObjectIndex( int start,
                                                                                      int end ) const
{
  assert( 0 <= end && end < 2 * BUFFER_SIZE &&
          "End index must be >= 0 and smaller than 2 * BUFFER_SIZE" );
  int index = start;
  if ( end >= BUFFER_SIZE )
    end -= BUFFER_SIZE; // Wrap around the end index
  int obj_index = -1;
  bool have_first = false;
  do {
    if ( have_first ) {
      if ( buffer_[index] == 0x42 )
        return obj_index;
      have_first = false;
    }
    if ( buffer_[index] == 0x02 ) {
      obj_index = index;
      have_first = true;
    }
    if ( ++index >= BUFFER_SIZE )
      index -= BUFFER_SIZE; // Wrap around the buffer index
  } while ( index != end );
  return -1; // No object found
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline int CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::available() const
{
  if ( buffer_size_ == 0 )
    return 0;
  int obj_index = _findNextObjectIndex( buffer_index_, buffer_index_ + buffer_size_ );
  if ( obj_index == -1 ) {
    int last_index = buffer_index_ + buffer_size_ - 1;
    if ( last_index >= BUFFER_SIZE )
      last_index -= BUFFER_SIZE; // Wrap around the buffer index
    // Check if last byte could be a start marker
    return buffer_[last_index] == 0x02 ? buffer_size_ - 1 : buffer_size_;
  }
  int available = obj_index - buffer_index_;
  if ( available < 0 )
    available += BUFFER_SIZE; // Wrap around the buffer index
  return available;
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline size_t CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::read( uint8_t *data, size_t length )
{
  if ( int available_bytes = available(); static_cast<int>( length ) > available_bytes )
    length = available_bytes;
  if ( length == 0 )
    return 0;

  int start = buffer_index_;
  int end = buffer_index_ + length;
  if ( end > BUFFER_SIZE ) {
    std::memcpy( data, &buffer_[start], BUFFER_SIZE - start );
    data += ( BUFFER_SIZE - start );
    start = 0;
    end -= BUFFER_SIZE; // Wrap around the buffer index
  }
  std::memcpy( data, &buffer_[start], end - start );
  _markRead( length );
  return length;
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline size_t CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::skip( size_t length )
{
  processSerialData( false );
  if ( int available_bytes = available(); static_cast<int>( length ) > available_bytes )
    length = available_bytes;
  _markRead( length );
  return length;
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline bool CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::hasObject() const
{
  if ( buffer_size_ < 4 || buffer_[buffer_index_] != 0x02 )
    return false;
  int second_index = buffer_index_ + 1;
  if ( second_index >= BUFFER_SIZE )
    second_index -= BUFFER_SIZE; // Wrap around the buffer index
  return buffer_[second_index] == 0x42;
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline int16_t CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::getObjectId() const
{
  if ( buffer_size_ < 4 || !hasObject() )
    return -1;
  // ID is the third and fourth byte in the serialized object
  int index = buffer_index_ + 2;
  if ( index >= BUFFER_SIZE )
    index -= BUFFER_SIZE;
  uint16_t tmp = 0;
  if ( index == BUFFER_SIZE - 1 ) {
    tmp = buffer_[index] | ( static_cast<uint16_t>( buffer_[0] ) << 8 );
  } else {
    std::memcpy( &tmp, &buffer_[index], 2 );
  }
  tmp = le16tohost( tmp );
  int16_t result;
  std::memcpy( &result, &tmp, sizeof( int16_t ) );
  return result;
}

namespace util
{

template<typename T, std::enable_if_t<std::is_scalar_v<T>, int> = 0>
constexpr size_t compute_size( const T & )
{
  return sizeof( T );
}

inline size_t compute_size( const std::string &str ) { return sizeof( uint16_t ) + str.length(); }

template<typename T, size_t N>
size_t compute_size( const std::array<T, N> &array );

template<typename T>
size_t compute_size( const std::vector<T> &vec );

template<typename T, std::enable_if_t<!std::is_scalar_v<T>, int> = 0>
size_t compute_size( const T &obj );

template<typename T, size_t N>
size_t compute_size( const std::array<T, N> &array )
{
  if constexpr ( std::is_scalar_v<T> ) {
    return sizeof( uint16_t ) + N * sizeof( T );
  } else {
    size_t size = sizeof( uint16_t ); // Size of the array length
    for ( const auto &item : array ) { size += compute_size( item ); }
    return size;
  }
}

template<typename T>
size_t compute_size( const std::vector<T> &vec )
{
  if constexpr ( std::is_scalar_v<T> ) {
    return sizeof( uint16_t ) + vec.size() * sizeof( T );
  } else {
    size_t size = sizeof( uint16_t ); // Size of the vector length
    for ( const auto &item : vec ) { size += compute_size( item ); }
    return size;
  }
}

template<typename T, std::enable_if_t<!std::is_scalar_v<T>, int>>
size_t compute_size( const T &obj )
{
  const size_t size = refl::util::accumulate(
      refl::reflect( obj ).members,
      [&]( size_t size, auto &&member ) { return size + compute_size( member( obj ) ); }, 0 );
  return size;
}

template<typename T, std::enable_if_t<std::is_scalar_v<T>, int> = 0>
size_t serialize( const T &value, uint8_t *data )
{
  constexpr size_t size = sizeof( T );
  if constexpr ( size == 1 ) {
    std::memcpy( data, &value, size );
  } else if constexpr ( size == 2 ) {
    uint16_t tmp = 0;
    std::memcpy( &tmp, &value, size );
    tmp = hosttole16( tmp );
    std::memcpy( data, &tmp, size );
  } else if constexpr ( size == 4 ) {
    uint32_t tmp = 0;
    std::memcpy( &tmp, &value, size );
    tmp = hosttole32( tmp );
    std::memcpy( data, &tmp, size );
  } else if constexpr ( size == 8 ) {
    uint64_t tmp = 0;
    std::memcpy( &tmp, &value, size );
    tmp = hosttole64( tmp );
    std::memcpy( data, &tmp, size );
  } else {
    static_assert( size <= 8, "Unsupported type size for serialization." );
  }
  return size;
}

template<typename T, std::enable_if_t<std::is_scalar_v<T>, int> = 0>
size_t deserialize( const uint8_t *data, int length, T &value )
{
  constexpr int size = sizeof( T );
  if ( length < size )
    return 0; // Not enough data to deserialize
  if constexpr ( size == 1 ) {
    std::memcpy( &value, data, size );
  } else if constexpr ( size == 2 ) {
    uint16_t tmp = 0;
    std::memcpy( &tmp, data, size );
    tmp = le16tohost( tmp );
    std::memcpy( &value, &tmp, size );
  } else if constexpr ( size == 4 ) {
    uint32_t tmp = 0;
    std::memcpy( &tmp, data, size );
    tmp = le32tohost( tmp );
    std::memcpy( &value, &tmp, size );
  } else if constexpr ( size == 8 ) {
    uint64_t tmp = 0;
    std::memcpy( &tmp, data, size );
    tmp = le64tohost( tmp );
    std::memcpy( &value, &tmp, size );
  } else {
    static_assert( size <= 8, "Unsupported type size for deserialization." );
  }
  return size;
}

inline size_t serialize( const std::string &str, uint8_t *data )
{
  size_t offset = serialize( uint16_t( str.length() ), data );
  std::memcpy( data + offset, str.data(), str.length() );
  return offset + str.length();
}

inline size_t deserialize( const uint8_t *data, int length, std::string &str )
{
  uint16_t str_length = 0;
  size_t offset = deserialize( data, length, str_length );
  if ( length < static_cast<int>( offset + str_length ) )
    return 0; // Not enough data to deserialize
  str.assign( reinterpret_cast<const char *>( data + offset ), str_length );
  return offset + str_length;
}

template<typename T>
size_t serialize( const std::vector<T> &vec, uint8_t *data );

template<typename T>
size_t deserialize( const uint8_t *data, int length, std::vector<T> &vec );

template<typename T, size_t N>
size_t serialize( const std::array<T, N> &array, uint8_t *data );

template<typename T, size_t N>
size_t deserialize( const uint8_t *data, int length, std::array<T, N> &array );

template<typename T, std::enable_if_t<!std::is_scalar_v<T>, int> = 0>
size_t serialize( const T &obj, uint8_t *data );

template<typename T, std::enable_if_t<!std::is_scalar_v<T>, int> = 0>
constexpr size_t deserialize( const uint8_t *data, int length, T &obj );

template<typename T>
size_t serialize( const std::vector<T> &vec, uint8_t *data )
{
  size_t offset = serialize( uint16_t( vec.size() ), data );
  for ( const auto &item : vec ) { offset += serialize( item, data + offset ); }
  return offset;
}

template<typename T>
size_t deserialize( const uint8_t *data, int length, std::vector<T> &vec )
{
  uint16_t item_count = 0;
  size_t offset = deserialize( data, length, item_count );
  vec.resize( item_count );
  for ( size_t i = 0; i < item_count; ++i ) {
    offset += deserialize( data + offset, length - offset, vec[i] );
  }
  return offset;
}

template<typename T, size_t N>
size_t serialize( const std::array<T, N> &array, uint8_t *data )
{
  size_t offset = serialize( uint16_t( N ), data );
  for ( const auto &item : array ) { offset += serialize( item, data + offset ); }
  return offset;
}

template<typename T, size_t N>
size_t deserialize( const uint8_t *data, int length, std::array<T, N> &array )
{
  uint16_t item_count = 0;
  size_t offset = deserialize( data, length, item_count );
  assert( item_count == N );
  for ( size_t i = 0; i < std::min<size_t>( N, item_count ); ++i ) {
    offset += deserialize( data + offset, length - offset, array[i] );
  }
  return offset;
}

template<typename T, std::enable_if_t<!std::is_scalar_v<T>, int>>
size_t serialize( const T &obj, uint8_t *data )
{
  static_assert( refl::is_reflectable<T>() && "Type must be reflectable." );
  size_t offset = 0;
  refl::util::for_each( refl::reflect( obj ).members, [&]( auto &&member ) {
    offset += serialize( member( obj ), data + offset );
  } );
  return offset;
}

template<typename T, std::enable_if_t<!std::is_scalar_v<T>, int>>
constexpr size_t deserialize( const uint8_t *data, int length, T &obj )
{
  static_assert( refl::is_reflectable<T>() && "Type must be reflectable." );
  size_t offset = 0;
  refl::util::for_each( refl::reflect( obj ).members, [&]( auto &&member ) {
    offset += deserialize( data + offset, length - offset, member( obj ) );
  } );
  return offset;
}

inline uint16_t compute_crc16( const uint8_t *data, size_t length )
{
  uint8_t x;
  uint16_t crc = 0xFFFF;
  for ( size_t i = 0; i < length; ++i ) {
    x = ( crc >> 8 ) ^ data[i];
    x ^= ( x >> 4 );
    crc = ( crc << 8 ) ^ ( x << 12 ) ^ ( x << 5 ) ^ x;
  }
  return crc;
}
} // namespace util

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
template<typename T>
inline ReadResult CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::readObject( T &obj )
{
  static_assert( refl::is_reflectable<T>(), "Type must be reflectable." );
  constexpr auto type_info = refl::reflect<T>();
  constexpr auto id = std::get<crosstalk::id>( type_info.attributes ).id_value;

  if ( !hasObject() ) {
    return ReadResult::NoObjectAvailable;
  }
  // Read as much data as available
  _processSerialDataUntil( buffer_index_ );
  if ( buffer_size_ < 6 ) {
    return ReadResult::NotEnoughData; // Not enough data to read metadata
  }
  if ( getObjectId() != id )
    return ReadResult::ObjectIdMismatch;
  uint16_t serialized_size = _readObjectSize( buffer_index_ );
  if ( serialized_size + 8 > buffer_size_ ) {
    return ReadResult::NotEnoughData; // Not enough data to deserialize the object
  }
  const uint8_t *data = &buffer_[buffer_index_];
  if ( buffer_index_ + serialized_size + 8 > BUFFER_SIZE ) {
    // If data wraps around circular buffer, copy it to read buffer for continuous access
    std::memcpy( obj_buffer_.data(), &buffer_[buffer_index_], BUFFER_SIZE - buffer_index_ );
    std::memcpy( obj_buffer_.data() + BUFFER_SIZE - buffer_index_, &buffer_[0],
                 buffer_index_ + 8 + serialized_size - BUFFER_SIZE );
    data = obj_buffer_.data();
  }

  // Check CRC
  uint16_t crc = 0;
  std::memcpy( &crc, data + serialized_size + 6, 2 );
  crc = le16tohost( crc );
  uint16_t computed_crc = util::compute_crc16( data, 6 + serialized_size );
  size_t consumed = 0;
  if ( crc == computed_crc ) {
    consumed = util::deserialize<T>( data + 6, serialized_size, obj );
  }
  // Whether or not the CRC is valid, we need to update the buffer indices
  _markRead( 8 + serialized_size );
  if ( crc != computed_crc )
    return ReadResult::CrcError;
  return serialized_size != consumed ? ReadResult::ObjectSizeMismatch : ReadResult::Success;
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
inline ReadResult CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::skipObject()
{
  if ( !hasObject() ) {
    return ReadResult::NoObjectAvailable;
  }
  _processSerialDataUntil( buffer_index_ );
  if ( buffer_size_ < 6 ) {
    return ReadResult::NotEnoughData; // Not enough data to read metadata
  }
  uint16_t serialized_size = _readObjectSize( buffer_index_ );
  if ( serialized_size + 8 > buffer_size_ ) {
    return ReadResult::NotEnoughData; // Not enough data to skip the object
  }
  _markRead( serialized_size + 8 );
  return ReadResult::Success;
}

template<int BUFFER_SIZE, int SERIALIZATION_BUFFER_SIZE>
template<typename T>
inline WriteResult CrossTalker<BUFFER_SIZE, SERIALIZATION_BUFFER_SIZE>::sendObject( const T &obj )
{
  static_assert( refl::is_reflectable<T>(), "Type must be reflectable." );
  constexpr auto type_info = refl::reflect<T>();
  constexpr auto id = std::get<crosstalk::id>( type_info.attributes ).id_value;
  static_assert( id >= 0, "Object ID must be greater or equal to 0. Negative ids are reserved." );
  // 2 bytes start, 2 byte id, 2 bytes length, 2 bytes crc
  size_t size = 8 + util::compute_size( obj );
  if ( size > SERIALIZATION_BUFFER_SIZE ) {
    return WriteResult::ObjectTooLarge;
  }
  obj_buffer_[0] = 0x02;
  obj_buffer_[1] = 0x42;
  // Write the ID in little-endian format
  uint16_t uid;
  std::memcpy( &uid, &id, sizeof( uint16_t ) );
  uid = hosttole16( uid );
  *reinterpret_cast<uint16_t *>( obj_buffer_.data() + 2 ) = uid;
  // Write the serialized object
  size_t serialized_size = util::serialize<T>( obj, obj_buffer_.data() + 6 );
  // Write the size of the serialized object
  *reinterpret_cast<uint16_t *>( obj_buffer_.data() + 4 ) =
      hosttole16( static_cast<uint16_t>( serialized_size ) );
  assert( serialized_size == size - 8 && "Serialized size does not match expected size" );
  // Write the CRC
  *reinterpret_cast<uint16_t *>( obj_buffer_.data() + 6 + serialized_size ) =
      hosttole16( util::compute_crc16( obj_buffer_.data(), 6 + serialized_size ) );
  return serial_->write( obj_buffer_.data(), size ) ? WriteResult::Success : WriteResult::WriteError;
}
} // namespace crosstalk

#endif // CROSSTALK_CROSSTALKER_HPP
