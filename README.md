# Crosstalk

Crosstalk is a small single-header C++ 17 library to facilitate communication between microcontrollers and host
computers
over serial connections.

It uses reflection (via [refl-cpp](https://github.com/veselink1/refl-cpp)) to automatically generate serialization and
deserialization code for user-defined types.
The objects are injected into the serial stream and crosstalk intelligently handles the reading of objects and generic
data.
This means you can keep your logging without having to worry about the serialization of your objects.

## Usage

Copy the `crosstalk.hpp` file and the relevant serial abstraction from the dist folder into your project and include it
in your code.
If there is no fitting serial abstraction for your platform, you can implement your own by inheriting from the
`crosstalk::SerialAbstraction` class.

## Example

### Shared code

```cpp
#include "crosstalk.hpp"

struct MyData {
  std::string name;
  float measurement;
  uint32_t timestamp;
};

// The id needs to be unique for each type you want to serialize.
REFL_AUTO(type(MyData, crosstalk::id(1)),
    field(name), field(measurement), field(timestamp))
```

### Microcontroller code (e.g. ESP32 using Arduino framework)

```cpp
#include "shared_code.hpp"
#include "crosstalk_hardware_serial_abstraction.hpp"

// The 512 is the maximum size of the serial buffer, the 256 the size of the
// serialization buffer. The sizes can be adjusted to your needs, but the
// serialization buffer should be large enough to hold the largest object and
// the serial buffer should be at least as large as the largest object you want
// to send.
crosstalk::CrossTalker<512, 256> crosstalk(
    std::make_unique<crosstalk::HardwareSerialWrapper<HWCDC>>(Serial));

void setup() {
  Serial.begin(115200);
  Serial.println("Initialized.");
}

void loop() {
  MyData data;
  data.name = "Sensor1";
  data.measurement = 42.0f;
  data.timestamp = millis();
  crosstalk.sendObject(data);
  delay(1000); // Send every second
}
```

### Host code (e.g. Linux using LibSerial)

```cpp
#include "shared_code.hpp"
#include "crosstalk_lib_serial_abstraction.hpp"

int main() {
  // Init serial (e.g. LibSerial)
  auto serial = ...;
  // Replace serial abstraction with one of the provided abstractions or implement your own
  crosstalk::CrossTalker crosstalker(std::make_unique<crosstalk::LibSerialWrapper>(serial));
  
  std::vector<uint8_t> buffer;
  while( ok ) {
    crosstalker.processSerialData(true); // Pass false if you don't want to lose data if the buffer is full
    if (crosstalker.available() > 0) {
      // Read generic data (e.g. from logging using Serial.println)
      buffer.resize(crosstalker.available());
      size_t bytes_read = crosstalker.read(buffer.data(), buffer.size());
      // E.g. print to console
      std::cout << std::string((const char*)buffer.data(), bytes_read);
      // Alternatively, you can use crosstalker.skip() to skip the data
    }
    if (!crosstalker.hasObject()) continue;
    switch (crosstalker.getObjectId()) {
      // To make it verbose and robust to changes of the id, we could also use `case 1:` if we're certain the id won't change
      case crosstalk::object_id<MyData>(): {
        MyData data;
        auto result = crosstalker.readObject(data);
        if (result != crosstalk::ReadObjectResult::Success) {
          std::cerr << "Failed to read MyData object with code " << static_cast<int>(result) << std::endl;
          break;
        }
        processMyData(data);
        break;
      }
      default:
        // This shouldn't happen if you handle all your types but we include it for completeness
        std::cerr << "Unknown object ID: " << crosstalker.getObjectId() << std::endl;
        crosstalker.skipObject();
        break;
    }
  }
}
```