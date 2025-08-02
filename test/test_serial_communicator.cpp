#include "crosstalk/crosstalker.hpp"
#include "test_objects.hpp"
#include "gtest/gtest.h"

class TestSerialAbstraction : public crosstalk::SerialAbstraction
{
public:
  TestSerialAbstraction( std::vector<uint8_t> &send_buffer, std::vector<uint8_t> &receive_buffer )
      : send_buffer( send_buffer ), receive_buffer( receive_buffer )
  {
  }

  int available() const override { return receive_buffer.size(); }

  int read( uint8_t *data, size_t length ) override
  {
    if ( receive_buffer.size() < length ) {
      length = receive_buffer.size();
    }
    std::memcpy( data, receive_buffer.data(), length );
    receive_buffer.erase( receive_buffer.begin(), receive_buffer.begin() + length );
    return length;
  }

  bool write( const uint8_t *data, size_t length ) override
  {
    send_buffer.insert( send_buffer.end(), data, data + length );
    return true; // Simulate successful write
  }

  std::vector<uint8_t> &send_buffer;
  std::vector<uint8_t> &receive_buffer;
};

TEST( SerialCommunicatorTest, serialization )
{
  std::vector<uint8_t> device_buffer;
  std::vector<uint8_t> host_buffer;
  crosstalk::CrossTalker<256, 256> comm1(
      std::make_unique<TestSerialAbstraction>( host_buffer, device_buffer ) );
  crosstalk::CrossTalker<256, 256> comm2(
      std::make_unique<TestSerialAbstraction>( device_buffer, host_buffer ) );
  ASSERT_FALSE( comm2.hasObject() );
  ASSERT_EQ( comm2.getObjectId(), -1 );

  ASSERT_EQ( comm1.sendObject( TestObjectSimple{ 42, 3.14f } ), crosstalk::WriteResult::Success );
  comm2.processSerialData(); // Simulate receiving data
  ASSERT_TRUE( comm2.hasObject() );
  TestObjectSimple obj = {};
  ASSERT_EQ( comm2.readObject( obj ), crosstalk::ReadResult::Success );
  EXPECT_EQ( obj.id, 42 );
  EXPECT_FLOAT_EQ( obj.value, 3.14f );

  ASSERT_FALSE( comm2.hasObject() );
  ASSERT_FALSE( comm1.hasObject() );
  ASSERT_EQ( comm1.getObjectId(), -1 );

  ASSERT_EQ( comm2.sendObject( TestObjectWithString{ 123, "TestName" } ),
             crosstalk::WriteResult::Success );
  device_buffer.push_back( 'A' ); // Simulate a byte that should not be part of the object
  comm1.processSerialData();
  ASSERT_TRUE( comm1.hasObject() );
  TestObjectWithString obj2;
  ASSERT_EQ( comm1.readObject( obj2 ), crosstalk::ReadResult::Success );
  EXPECT_EQ( obj2.uuid, 123 );
  EXPECT_EQ( obj2.name, "TestName" );

  ASSERT_EQ( comm1.available(), 1 );
  std::vector<uint8_t> data;
  data.resize( 1 );
  comm1.read( data.data(), data.size() );
  ASSERT_EQ( data[0], 'A' ); // Check that the extra byte is still there
  comm1.processSerialData();
  ASSERT_EQ( comm1.available(), 0 );

  ASSERT_FALSE( comm1.hasObject() );
  ASSERT_FALSE( comm2.hasObject() );

  ASSERT_EQ(
      comm1.sendObject( TestWithSimpleVectorAndArray{ 3.14159f, { 1, 2, 3 }, { 4.0, 5.0, 6.0 } } ),
      crosstalk::WriteResult::Success );
  host_buffer.insert( host_buffer.end(), { 'E', 'X', 'T', 'R', 'A' } ); // Extra bytes
  comm2.processSerialData();
  ASSERT_TRUE( comm2.hasObject() );
  TestWithSimpleVectorAndArray obj3;
  ASSERT_EQ( comm2.readObject( obj3 ), crosstalk::ReadResult::Success );
  EXPECT_FLOAT_EQ( obj3.pi, 3.14159f );
  EXPECT_EQ( obj3.numbers, ( std::vector<int>{ 1, 2, 3 } ) );
  EXPECT_EQ( obj3.coordinates, ( std::array<double, 3>{ 4.0, 5.0, 6.0 } ) );

  ASSERT_EQ( comm2.available(), 5 );
  data.resize( comm2.available() );
  comm2.read( data.data(), data.size() );
  ASSERT_EQ( data, ( std::vector<uint8_t>{ 'E', 'X', 'T', 'R', 'A' } ) ); // Check extra bytes

  ASSERT_FALSE( comm2.hasObject() );
  ASSERT_FALSE( comm1.hasObject() );

  // Test wrap around. Fill with dummy data to simulate a full buffer
  device_buffer.resize( 250 );
  std::fill( device_buffer.begin(), device_buffer.end(), 0xFF );

  ASSERT_EQ( comm2.sendObject( TestWithComplexVectorAndArray{
                 "uuid-123",
                 { "name1", "name2" },
                 { std::vector<int>{ 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 } } } ),
             crosstalk::WriteResult::Success );
  comm1.processSerialData();
  ASSERT_FALSE( comm1.hasObject() ); // Should not have an object yet
  comm1.skip();                      // Skip until the next object start marker
  ASSERT_TRUE( comm1.hasObject() );
  TestWithComplexVectorAndArray obj4;
  ASSERT_EQ( comm1.readObject( obj4 ), crosstalk::ReadResult::Success );
  EXPECT_EQ( obj4.uuid, "uuid-123" );
  EXPECT_EQ( obj4.names, ( std::vector<std::string>{ "name1", "name2" } ) );
  EXPECT_EQ( obj4.vectors, ( std::array<std::vector<int>, 3>{ std::vector<int>{ 1, 2, 3 },
                                                              std::vector<int>{ 4, 5, 6 },
                                                              std::vector<int>{ 7, 8, 9 } } ) );

  ASSERT_FALSE( comm1.hasObject() );
  ASSERT_FALSE( comm2.hasObject() );
  comm1.processSerialData();
  ASSERT_EQ( comm1.available(), 0 );

  ASSERT_EQ(
      comm1.sendObject( TestWithClassVectorAndArray{
          456,
          { TestWithComplexVectorAndArray{ "uuid-456",
                                           { "nameA", "nameB" },
                                           { std::vector<int>{ 10, 11 }, { 12, 13 }, { 14, 15 } } },
            TestWithComplexVectorAndArray{
                "uuid-789", { "nameC" }, { std::vector<int>{ 16, 17, 18 }, {} } } },
          { TestObjectWithString{ 789, "Object1" }, TestObjectWithString{ 101112, "Object2" },
            TestObjectWithString{ 131415, "Object3" } } } ),
      crosstalk::WriteResult::Success );
  comm2.processSerialData();
  ASSERT_TRUE( comm2.hasObject() );
  TestWithClassVectorAndArray obj5;
  ASSERT_EQ( comm2.readObject( obj5 ), crosstalk::ReadResult::Success );
  EXPECT_EQ( obj5.id, 456 );
  ASSERT_EQ( obj5.objects.size(), 2 );
  EXPECT_EQ( obj5.objects[0].uuid, "uuid-456" );
  EXPECT_EQ( obj5.objects[0].names, ( std::vector<std::string>{ "nameA", "nameB" } ) );
  EXPECT_EQ( obj5.objects[0].vectors,
             ( std::array<std::vector<int>, 3>{ std::vector<int>{ 10, 11 }, std::vector<int>{ 12, 13 },
                                                std::vector<int>{ 14, 15 } } ) );
  EXPECT_EQ( obj5.objects[1].uuid, "uuid-789" );
  EXPECT_EQ( obj5.objects[1].names, ( std::vector<std::string>{ "nameC" } ) );
  EXPECT_EQ( obj5.objects[1].vectors,
             ( std::array<std::vector<int>, 3>{ std::vector<int>{ 16, 17, 18 }, {} } ) );
  ASSERT_EQ( obj5.object_array.size(), 3 );
  EXPECT_EQ( obj5.object_array[0].uuid, 789 );
  EXPECT_EQ( obj5.object_array[0].name, "Object1" );
  EXPECT_EQ( obj5.object_array[1].uuid, 101112 );
  EXPECT_EQ( obj5.object_array[1].name, "Object2" );
  EXPECT_EQ( obj5.object_array[2].uuid, 131415 );
  EXPECT_EQ( obj5.object_array[2].name, "Object3" );
  ASSERT_FALSE( comm2.hasObject() );

  // Test case with actual object

  ASSERT_FALSE( comm1.hasObject() );
  ASSERT_EQ( comm1.sendObject( CommStatus{
                 1378, -98.0f, -85.0f, 0.0f, CommQuality::NONE, CommQuality::NONE, CommQuality::NONE,
                 CommState::DISCONNECTED, CommState::DISCONNECTED, CommState::DISCONNECTED } ),
             crosstalk::WriteResult::Success );
  comm2.processSerialData();
  ASSERT_TRUE( comm2.hasObject() );
  CommStatus comm_status;
  ASSERT_EQ( comm2.readObject( comm_status ), crosstalk::ReadResult::Success );
  EXPECT_EQ( comm_status.last_received_message_age_ms, 1378 );
  EXPECT_FLOAT_EQ( comm_status.ble_rssi, -98.0f );
  EXPECT_FLOAT_EQ( comm_status.radio_rssi, -85.0f );
  EXPECT_FLOAT_EQ( comm_status.esp_now_rssi, 0.0f );
  EXPECT_EQ( comm_status.ble_quality, CommQuality::NONE );
  EXPECT_EQ( comm_status.radio_quality, CommQuality::NONE );
  EXPECT_EQ( comm_status.esp_now_quality, CommQuality::NONE );
  EXPECT_EQ( comm_status.ble_state, CommState::DISCONNECTED );
  EXPECT_EQ( comm_status.esp_now_state, CommState::DISCONNECTED );
  EXPECT_EQ( comm_status.radio_state, CommState::DISCONNECTED );
  ASSERT_FALSE( comm1.hasObject() );
  ASSERT_FALSE( comm2.hasObject() );
}

TEST( SerialCommunicatorTest, errors )
{
  std::vector<uint8_t> device_buffer;
  std::vector<uint8_t> host_buffer;
  crosstalk::CrossTalker<128> comm1(
      std::make_unique<TestSerialAbstraction>( host_buffer, device_buffer ) );
  crosstalk::CrossTalker<128> comm2(
      std::make_unique<TestSerialAbstraction>( device_buffer, host_buffer ) );

  TestWithSimpleVectorAndArray obj{ 3.14159f, { 1, 2, 3 }, { 4.0, 5.0, 6.0 } };
  ASSERT_EQ( comm1.sendObject( obj ), crosstalk::WriteResult::Success );
  host_buffer.pop_back();
  comm2.processSerialData();
  ASSERT_TRUE( comm2.hasObject() );
  TestWithSimpleVectorAndArray obj2;
  auto result = comm2.readObject( obj2 );
  ASSERT_EQ( result, crosstalk::ReadResult::NotEnoughData );
  host_buffer.push_back( 0 ); // Wrong last byte
  comm2.processSerialData();
  result = comm2.readObject( obj2 );
  ASSERT_EQ( result, crosstalk::ReadResult::CrcError );
  ASSERT_FALSE( comm2.hasObject() );

  for ( uint8_t i = 'A'; i <= 'Z'; ++i ) {
    host_buffer.push_back( static_cast<uint8_t>( i ) ); // Add some random data
  }
  comm2.processSerialData();
  ASSERT_EQ( comm2.available(), 26 );
  ASSERT_EQ( comm1.sendObject( obj ), crosstalk::WriteResult::Success );
  host_buffer[host_buffer.size() / 2] ^= 0x42; // Corrupt some data
  host_buffer.push_back( 'T' );
  host_buffer.push_back( 'E' );
  comm2.processSerialData();
  std::vector<uint8_t> data;
  data.resize( comm2.available() );
  comm2.read( data.data(), data.size() );
  ASSERT_EQ( data.size(), 26 );
  for ( uint8_t i = 'A', index = 0; i <= 'Z'; ++i, ++index ) { ASSERT_EQ( data[index], i ); }
  ASSERT_TRUE( comm2.hasObject() );
  ASSERT_EQ( comm2.readObject( obj2 ), crosstalk::ReadResult::CrcError );
  ASSERT_EQ( comm2.available(), 2 );
  comm2.skip( 2 );
  ASSERT_EQ( comm2.available(), 0 );

  ASSERT_EQ( comm1.sendObject( obj ), crosstalk::WriteResult::Success );
  host_buffer.resize( 5 ); // Simulate not enough data for metadata
  comm2.processSerialData();
  ASSERT_TRUE( comm2.hasObject() );
  result = comm2.readObject( obj2 );
  ASSERT_EQ( result, crosstalk::ReadResult::NotEnoughData );
  comm2.clearBuffer();

  host_buffer.assign( { 0x01, 0x02, 0x03, 0x04 } );
  comm2.processSerialData();
  ASSERT_FALSE( comm2.hasObject() );
  result = comm2.readObject( obj2 );
  ASSERT_EQ( result, crosstalk::ReadResult::NoObjectAvailable );
  comm2.skip( 4 );
  ASSERT_EQ( comm1.sendObject( obj ), crosstalk::WriteResult::Success );
  comm2.processSerialData();
  TestObjectSimple wrong_object = {};
  result = comm2.readObject( wrong_object );
  ASSERT_EQ( result, crosstalk::ReadResult::ObjectIdMismatch );

  auto send_result = comm1.sendObject( TestWithClassVectorAndArray{
      456,
      { TestWithComplexVectorAndArray{
            "uuid-456", { "nameA", "nameB" }, { std::vector<int>{ 10, 11 }, { 12, 13 }, { 14, 15 } } },
        TestWithComplexVectorAndArray{
            "uuid-789", { "nameC" }, { std::vector<int>{ 16, 17, 18 }, {} } } },
      { TestObjectWithString{ 789, "Object1" }, TestObjectWithString{ 101112, "Object2" },
        TestObjectWithString{ 131415, "Object3" } } } );
  ASSERT_EQ( send_result, crosstalk::WriteResult::ObjectTooLarge );
}

TEST( SerialCommunicatorTest, wrapping )
{
  std::vector<uint8_t> device_buffer;
  std::vector<uint8_t> host_buffer;
  crosstalk::CrossTalker<32> comm1(
      std::make_unique<TestSerialAbstraction>( host_buffer, device_buffer ) );
  crosstalk::CrossTalker<32> comm2(
      std::make_unique<TestSerialAbstraction>( device_buffer, host_buffer ) );
  TestObjectSimple obj = {};
  std::vector<uint8_t> data;

  // Marker at last byte
  device_buffer.resize( 31 );
  std::fill( device_buffer.begin(), device_buffer.end(), 0xFF );
  comm1.processSerialData();
  ASSERT_GT( comm1.available(), 0 );
  comm2.sendObject( TestObjectSimple{ 42, 3.14f } );
  comm1.processSerialData();
  ASSERT_FALSE( comm1.hasObject() );
  data.resize( comm1.available() );
  EXPECT_EQ( comm1.read( data.data(), 0 ), 0 );
  ASSERT_EQ( comm1.available(), data.size() );
  EXPECT_EQ( comm1.read( data.data(), data.size() + 4 ), data.size() );
  ASSERT_TRUE( comm1.hasObject() );
  ASSERT_EQ( comm1.readObject( obj ), crosstalk::ReadResult::Success );

  // Id at last byte
  comm1.clearBuffer();
  device_buffer.resize( 29 );
  std::fill( device_buffer.begin(), device_buffer.end(), 0xFF );
  comm1.processSerialData();
  comm2.sendObject( TestObjectSimple{ 43, 2.71f } );
  comm1.processSerialData();
  comm1.skip();
  ASSERT_EQ( comm1.readObject( obj ), crosstalk::ReadResult::Success );

  // Size at last byte
  comm1.clearBuffer();
  device_buffer.resize( 27 );
  std::fill( device_buffer.begin(), device_buffer.end(), 0xFF );
  comm1.processSerialData();
  ASSERT_GT( comm1.available(), 0 );
  comm2.sendObject( TestObjectSimple{ 44, 1.41f } );
  comm1.processSerialData();
  comm1.skip();
  ASSERT_EQ( comm1.readObject( obj ), crosstalk::ReadResult::Success );

  // Lots of random data, object at the end
  device_buffer.resize( 96 );
  std::fill( device_buffer.begin(), device_buffer.end(), 0xFF );
  comm1.processSerialData();
  comm2.sendObject( TestObjectSimple{ 46, 0.618f } );
  comm1.processSerialData();
  // This should be four full buffers that have to be skipped.
  for ( int i = 0; i < 3; ++i ) {
    ASSERT_FALSE( comm1.hasObject() );
    comm1.skip();
  }
  ASSERT_TRUE( comm1.hasObject() );

  // When an object is available, it has to be read before the other serial data is available
  comm1.clearBuffer();
  comm2.sendObject( TestObjectSimple{ 47, 0.707f } );
  device_buffer.push_back( 17 );
  comm1.processSerialData();
  ASSERT_EQ( comm1.available(), 0 );
  ASSERT_TRUE( comm1.hasObject() );
  ASSERT_EQ( comm1.readObject( obj ), crosstalk::ReadResult::Success );
  ASSERT_EQ( comm1.available(), 1 );

  comm1.clearBuffer();
  device_buffer.resize( 32 );
  std::fill( device_buffer.begin(), device_buffer.end(), 0xFF );
  comm1.processSerialData();
  data.resize( 16 );
  ASSERT_EQ( comm1.available(), 32 );
  comm1.read( data.data(), data.size() );
  ASSERT_EQ( comm1.available(), 16 );
  for ( int i = 0; i < 16; ++i ) { device_buffer.push_back( static_cast<uint8_t>( i ) ); }
  comm1.processSerialData();
  ASSERT_EQ( comm1.available(), 32 );
  data.resize( 32 );
  ASSERT_EQ( comm1.read( data.data(), data.size() ), 32 );
  ASSERT_EQ( data[0], 0xFF );
  for ( int i = 16; i < 32; ++i ) { EXPECT_EQ( data[i], static_cast<uint8_t>( i - 16 ) ); }
}

int main( int argc, char **argv )
{
  ::testing::InitGoogleTest( &argc, argv );
  return RUN_ALL_TESTS();
}
