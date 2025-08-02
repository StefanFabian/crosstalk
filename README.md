[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=StefanFabian_crosstalk&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=StefanFabian_crosstalk)
[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=StefanFabian_crosstalk&metric=sqale_rating)](https://sonarcloud.io/summary/new_code?id=StefanFabian_crosstalk)
[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=StefanFabian_crosstalk&metric=reliability_rating)](https://sonarcloud.io/summary/new_code?id=StefanFabian_crosstalk)
[![Security Rating](https://sonarcloud.io/api/project_badges/measure?project=StefanFabian_crosstalk&metric=security_rating)](https://sonarcloud.io/summary/new_code?id=StefanFabian_crosstalk)

![img](./crosstalk-logo-wide.png "Crosstalk Logo")
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

Copy the `crosstalk.hpp` file and the relevant serial abstraction from the `dist` folder into your project and include it
in your code.
If there is no fitting serial abstraction for your platform, you can implement your own by implementing the
`crosstalk::SerialAbstraction` interface.

## Example

Generally, the sender can just send objects of all types that are registered with `REFL_AUTO`.
Make sure both the sender and receiver use the same type definitions and that the IDs are unique across all types.

>[!NOTE]
> The provided serial abstractions will only send objects if enough space is available in the serial write buffer.
> This avoids blocking writes at the cost of potential data loss if the serial buffer is full.
> If you do not want this, you can change the write method of the serial abstraction to always write, but this may lead
> to spiky loop execution times on the microcontroller if the serial buffer is not emptied quickly enough.

On the receiver side, you need to follow the following structure in a loop:
1. call `processSerialData()` to read the serial data into the internal buffer.
2. You can then check for and handle generic serial data using `available()` and `read()`, or just ignore it by calling `skip()`.
3. After that, you can check if an object is available using `hasObject()` and read it using `readObject()`.
4. Repeat.

### Shared code

```cpp
// shared_code.hpp
#include "crosstalk.hpp"

struct MyData {
  std::string name;
  float measurement;
  uint32_t timestamp; // Make sure to use types with fixed size! Avoid: int, long, etc.
};

// The id needs to be unique for each type you want to serialize.
REFL_AUTO(type(MyData, crosstalk::id(1)),
    field(name), field(measurement), field(timestamp))
```

>[!WARNING]
> Only use numerical types with a fixed size such as `int32_t`, `uint32_t`, `float`, `double`, etc.  
> Avoid using types like `int` or `long` as their size may vary across platforms.

### Microcontroller code (e.g. ESP32 using Arduino framework)

```cpp
#include "shared_code.hpp"
#include "crosstalk_hardware_serial_wrapper.hpp"

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
#include "crosstalk_lib_serial_wrapper.hpp"

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
      // `skip` will only skip serial data that is not part of an object
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
        crosstalker.skipObject(); //< This is important to avoid getting stuck on unknown objects
        break;
    }
  }
}
```

## Documentation

### `crosstalk::CrossTalker`

The `CrossTalker` class manages a circular buffer for serial data, handles object serialization/deserialization, and provides methods for reading/writing both generic data and typed objects over a serial connection.

#### Template Parameters

- `BUFFER_SIZE`: Size of the internal circular buffer for serial data (default: 512).
- `SERIALIZATION_BUFFER_SIZE`: Size of the buffer used for (de)serialization (default: `BUFFER_SIZE / 2`). Needed if object is wrapped around in circular buffer.

#### Constructor

```cpp
CrossTalker(std::unique_ptr<SerialAbstraction> serial);
```
- Initializes the CrossTalker with a serial abstraction.

#### Methods

- `void processSerialData(bool overwrite_buffer = true);`
  - Reads available serial data into the internal buffer.
  - If `overwrite_buffer` is true, old data may be overwritten if the buffer is full.

- `int available() const;`
  - Returns the number of bytes of generic (non-object) data available to read.

- `bool hasObject() const;`
  - Returns true if the start of an object is available next in the buffer.

- `int16_t getObjectId() const;`
  - Returns the ID of the next available object, or -1 if no object is available.

- `void clearBuffer();`
  - Clears the internal buffer and resets indices.

- `size_t read(uint8_t *data, size_t length);`
  - Reads up to `length` bytes of generic (non-object) data from the buffer into `data`.
  - Returns the number of bytes actually read.

- `size_t skip(size_t length = BUFFER_SIZE);`
  - Skips up to `length` bytes of generic data in the buffer (does not skip objects).

- `template<typename T> ReadResult readObject(T &obj);`
  - Attempts to read and deserialize an object of type `T` from the buffer.
  - Returns a `ReadResult` indicating success or the type of failure.

- `ReadResult skipObject();`
  - Skips the next available object in the buffer.
  - Returns a `ReadResult` indicating success or the type of failure.

- `template<typename T> WriteResult sendObject(const T &obj);`
  - Serializes and sends an object of type `T` over the serial connection.
  - Returns a `WriteResult` indicating success or the type of failure.

#### Enums

- `enum class ReadResult`
  - `Success`: Object was read successfully.
  - `NoObjectAvailable`: No object is currently available.
  - `NotEnoughData`: Not enough data to read or deserialize the object.
  - `CrcError`: CRC check failed.
  - `ObjectIdMismatch`: The object ID does not match the type you are trying to read.
  - `ObjectSizeMismatch`: The deserialized size does not match the expected size.

- `enum class WriteResult`
  - `Success`: Object was sent successfully.
  - `ObjectTooLarge`: The object is too large for the serialization buffer.
  - `WriteError`: An error occurred while writing to the serial connection.

All enums can be printed using `crosstalk::to_string(...)`.
