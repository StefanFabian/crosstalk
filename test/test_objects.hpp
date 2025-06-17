//
// Created by stefan on 13.06.25.
//

#ifndef SERIALLIBRARY_TEST_OBJECTS_HPP
#define SERIALLIBRARY_TEST_OBJECTS_HPP

struct TestObjectSimple {
  int id;
  float value;
};

REFL_AUTO( type( TestObjectSimple, crosstalk::id( 1 ) ), field( id ), field( value ) )

struct TestObjectWithString {
  int uuid;
  std::string name;
};

REFL_AUTO( type( TestObjectWithString, crosstalk::id( 2 ) ), field( uuid ), field( name ) )

struct TestWithSimpleVectorAndArray {
  float pi;
  std::vector<int> numbers;
  std::array<double, 3> coordinates;
};

REFL_AUTO( type( TestWithSimpleVectorAndArray, crosstalk::id( 3 ) ), field( pi ), field( numbers ),
    field( coordinates ) )

struct TestWithComplexVectorAndArray {
  std::string uuid;
  std::vector<std::string> names;
  std::array<std::vector<int>, 3> vectors;
};

REFL_AUTO( type( TestWithComplexVectorAndArray, crosstalk::id( 4 ) ), field( uuid ),
    field( names ), field( vectors ) )

struct TestWithClassVectorAndArray {
  uint16_t id;
  std::vector<TestWithComplexVectorAndArray> objects;
  std::array<TestObjectWithString, 3> object_array;
};

REFL_AUTO( type( TestWithClassVectorAndArray, crosstalk::id( 5 ) ), field( id ), field( objects ),
    field( object_array ) )

enum class CommQuality : uint8_t {
  NONE,
  LOW_QUALITY,
  MEDIUM_QUALITY,
  HIGH_QUALITY,
};

enum class CommState : uint8_t {
  DISCONNECTED = 0,
  CONNECTED = 1,
  ERROR = 10,
};

struct CommStatus {
  uint64_t last_received_message_age_ms = 0;
  float ble_rssi = 0.0f;
  float radio_rssi = 0.0f;
  float esp_now_rssi = 0.0f;
  CommQuality ble_quality = CommQuality::NONE;
  CommQuality radio_quality = CommQuality::NONE;
  CommQuality esp_now_quality = CommQuality::NONE;
  CommState ble_state = CommState::DISCONNECTED;
  CommState esp_now_state = CommState::DISCONNECTED;
  CommState radio_state = CommState::DISCONNECTED;
};

REFL_AUTO( type( CommStatus, crosstalk::id( 6 ) ), field( last_received_message_age_ms ),
    field( ble_rssi ), field( radio_rssi ), field( esp_now_rssi ), field( ble_quality ),
    field( radio_quality ), field( esp_now_quality ), field( ble_state ),
    field( esp_now_state ), field( radio_state ) )

#endif //SERIALLIBRARY_TEST_OBJECTS_HPP
