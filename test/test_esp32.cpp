#include "crosstalk/crosstalker.hpp"
#include "crosstalk/serial_abstractions/crosstalk_lib_serial_wrapper.hpp"
#include "libserial/SerialPort.h"
#include "test_objects.hpp"
#include "gtest/gtest.h"

using namespace std::chrono;
using namespace std::chrono_literals;

bool waitFor( crosstalk::CrossTalker<256, 256> &crosstalker, std::function<bool()> pred,
              std::chrono::milliseconds timeout = 100ms )
{
  steady_clock::time_point start = steady_clock::now();
  while ( steady_clock::now() - start < timeout ) {
    crosstalker.processSerialData();
    if ( pred() ) {
      return true;
    }
    usleep( 1000 ); // Sleep for 1ms
  }
  return false;
}

TEST( ESP32Test, communication )
{
  LibSerial::SerialPort serial_port;
  serial_port.Open( "/dev/ttyACM0" );
  serial_port.SetBaudRate( LibSerial::BaudRate::BAUD_115200 );
  crosstalk::CrossTalker<256, 256> comm(
      std::make_unique<crosstalk::LibSerialWrapper>( serial_port ) );
  serial_port.WriteByte( uint8_t( 0x42 ) );
  serial_port.FlushIOBuffers();
  std::vector<uint8_t> buffer;

  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.available() > 0; }, 3s ) );
  buffer.resize( comm.available() );
  ASSERT_EQ( comm.read( buffer.data(), buffer.size() ), buffer.size() );
  EXPECT_EQ( std::string( (char *)buffer.data(), buffer.size() ), "Test started!" );

  TestObjectSimple obj1;
  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.hasObject(); }, 100ms ) );
  ASSERT_EQ( comm.readObject( obj1 ), crosstalk::ReadResult::Success );
  EXPECT_EQ( obj1.id, 42 );
  EXPECT_FLOAT_EQ( obj1.value, 3.14f );

  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.available() > 0; }, 50ms ) );
  buffer.resize( comm.available() );
  ASSERT_EQ( comm.read( buffer.data(), buffer.size() ), buffer.size() );
  EXPECT_EQ( std::string( (char *)buffer.data(), buffer.size() ), "SUCCESS" );

  TestObjectWithString obj2;
  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.hasObject(); }, 100ms ) );
  ASSERT_EQ( comm.readObject( obj2 ), crosstalk::ReadResult::Success );
  EXPECT_EQ( obj2.uuid, 123 );
  EXPECT_EQ( obj2.name, "TestName" );

  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.available() > 0; }, 50ms ) );
  buffer.resize( comm.available() );
  ASSERT_EQ( comm.read( buffer.data(), buffer.size() ), buffer.size() );
  EXPECT_EQ( std::string( (char *)buffer.data(), buffer.size() ), "SUCCESS" );

  TestWithSimpleVectorAndArray obj3;
  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.hasObject(); }, 100ms ) );
  ASSERT_EQ( comm.readObject( obj3 ), crosstalk::ReadResult::Success );
  EXPECT_FLOAT_EQ( obj3.pi, 3.14159f );
  EXPECT_EQ( obj3.numbers, ( std::vector<int>{ 1, 2, 3 } ) );
  EXPECT_EQ( obj3.coordinates, ( std::array<double, 3>{ 4.0, 5.0, 6.0 } ) );

  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.available() > 0; }, 50ms ) );
  buffer.resize( comm.available() );
  ASSERT_EQ( comm.read( buffer.data(), buffer.size() ), buffer.size() );
  EXPECT_EQ( std::string( (char *)buffer.data(), buffer.size() ), "SUCCESS" );

  TestWithComplexVectorAndArray obj4;
  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.hasObject(); }, 100ms ) );
  ASSERT_EQ( comm.readObject( obj4 ), crosstalk::ReadResult::Success );
  EXPECT_EQ( obj4.uuid, "uuid-123" );
  EXPECT_EQ( obj4.names, ( std::vector<std::string>{ "name1", "name2" } ) );
  EXPECT_EQ( obj4.vectors, ( std::array<std::vector<int>, 3>{ std::vector<int>{ 1, 2, 3 },
                                                              std::vector<int>{ 4, 5, 6 },
                                                              std::vector<int>{ 7, 8, 9 } } ) );
  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.available() > 0; }, 50ms ) );
  buffer.resize( comm.available() );
  ASSERT_EQ( comm.read( buffer.data(), buffer.size() ), buffer.size() );
  EXPECT_EQ( std::string( (char *)buffer.data(), buffer.size() ), "SUCCESS" );

  TestWithClassVectorAndArray obj5;
  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.hasObject(); }, 100ms ) );
  ASSERT_EQ( comm.readObject( obj5 ), crosstalk::ReadResult::Success );
  EXPECT_EQ( obj5.id, 456 );
  EXPECT_EQ( obj5.objects.size(), 2 );
  EXPECT_EQ( obj5.objects[0].uuid, "uuid-456" );
  EXPECT_EQ( obj5.objects[0].names, ( std::vector<std::string>{ "nameA", "nameB" } ) );
  EXPECT_EQ( obj5.objects[0].vectors,
             ( std::array<std::vector<int>, 3>{ std::vector<int>{ 10, 11 }, std::vector<int>{ 12, 13 },
                                                std::vector<int>{ 14, 15 } } ) );

  EXPECT_EQ( obj5.objects[1].uuid, "uuid-789" );
  EXPECT_EQ( obj5.objects[1].names, ( std::vector<std::string>{ "nameC" } ) );
  EXPECT_EQ( obj5.objects[1].vectors,
             ( std::array<std::vector<int>, 3>{ std::vector<int>{ 16, 17, 18 }, std::vector<int>{},
                                                std::vector<int>{} } ) );
  EXPECT_EQ( obj5.object_array[0].uuid, 789 );
  EXPECT_EQ( obj5.object_array[0].name, "Object1" );
  EXPECT_EQ( obj5.object_array[1].uuid, 101112 );
  EXPECT_EQ( obj5.object_array[1].name, "Object2" );
  EXPECT_EQ( obj5.object_array[2].uuid, 131415 );
  EXPECT_EQ( obj5.object_array[2].name, "Object3" );
  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.available() > 0; }, 50ms ) );
  buffer.resize( comm.available() );
  ASSERT_EQ( comm.read( buffer.data(), buffer.size() ), buffer.size() );
  EXPECT_EQ( std::string( (char *)buffer.data(), buffer.size() ), "SUCCESS" );

  CommStatus obj6;
  ASSERT_TRUE( waitFor( comm, [&comm]() { return comm.hasObject(); }, 100ms ) );
  ASSERT_EQ( comm.readObject( obj6 ), crosstalk::ReadResult::Success );
  EXPECT_EQ( obj6.last_received_message_age_ms, 1378 );
  EXPECT_FLOAT_EQ( obj6.ble_rssi, -98.0f );
  EXPECT_FLOAT_EQ( obj6.radio_rssi, -85.0f );
  EXPECT_FLOAT_EQ( obj6.esp_now_rssi, 0.0f );
  EXPECT_EQ( obj6.ble_quality, CommQuality::NONE );
  EXPECT_EQ( obj6.radio_quality, CommQuality::MEDIUM_QUALITY );
  EXPECT_EQ( obj6.esp_now_quality, CommQuality::HIGH_QUALITY );
  EXPECT_EQ( obj6.ble_state, CommState::DISCONNECTED );
  EXPECT_EQ( obj6.esp_now_state, CommState::CONNECTED );
  EXPECT_EQ( obj6.radio_state, CommState::CONNECTED );
}

int main( int argc, char **argv )
{
  ::testing::InitGoogleTest( &argc, argv );
  return RUN_ALL_TESTS();
}
