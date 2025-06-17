#include "../../../dist/crosstalk.hpp"
#include "../../../dist/serial_abstractions/crosstalk_hardware_serial_wrapper.hpp"
#include "../../test_objects.hpp"
#include <Arduino.h>

using namespace crosstalk;

void toggleLED() { digitalWrite( LED_BUILTIN, !digitalRead( LED_BUILTIN ) ); }

template<typename Msg>
void sendTestObject( CrossTalker<512, 256> &crosstalker, const Msg &msg )
{
  auto result = crosstalker.sendObject( msg );
  if ( result == WriteResult::Success ) {
    Serial.print( "SUCCESS" );
  } else {
    Serial.print( "FAIL" );
  }
  toggleLED();
  delay( 20 );
}

void setup()
{
  pinMode( GPIO_NUM_21, INPUT ); // Reset button
  pinMode( LED_BUILTIN, OUTPUT );
  digitalWrite( LED_BUILTIN, LOW );
  Serial.begin( 115200 );
  while ( !Serial.available() || Serial.read() != 0x42 ) {
    toggleLED();
    delay( 1000 );
  }

  CrossTalker<512, 256> crosstalker( std::make_unique<HardwareSerialWrapper<HWCDC>>( Serial ) );
  Serial.print( "Test started!" );
  toggleLED();

  sendTestObject( crosstalker, TestObjectSimple{ 42, 3.14f } );
  sendTestObject( crosstalker, TestObjectWithString{ 123, "TestName" } );
  sendTestObject( crosstalker, TestWithSimpleVectorAndArray{ 3.14159f, { 1, 2, 3 }, { 4.0, 5.0, 6.0 } } );
  sendTestObject( crosstalker, TestWithComplexVectorAndArray{
      "uuid-123", { "name1", "name2" }, { std::vector<int>{ 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 } } } );
  sendTestObject( crosstalker, TestWithClassVectorAndArray{
      456,
      { TestWithComplexVectorAndArray{
            "uuid-456", { "nameA", "nameB" }, { std::vector<int>{ 10, 11 }, { 12, 13 }, { 14, 15 } } },
        TestWithComplexVectorAndArray{
            "uuid-789", { "nameC" }, { std::vector<int>{ 16, 17, 18 }, {} } } },
      { TestObjectWithString{ 789, "Object1" }, TestObjectWithString{ 101112, "Object2" },
        TestObjectWithString{ 131415, "Object3" } } } );
  sendTestObject( crosstalker, CommStatus{
      1378, -98.0f, -85.0f, 0.0f, CommQuality::NONE, CommQuality::MEDIUM_QUALITY, CommQuality::HIGH_QUALITY,
      CommState::DISCONNECTED, CommState::CONNECTED, CommState::CONNECTED } );

  digitalWrite( LED_BUILTIN, HIGH );
}

void loop()
{
  // Restart test if reset button is pressed
  if ( digitalRead( GPIO_NUM_21 ) == LOW ) {
    setup();
  }
  delay( 200 );
}
