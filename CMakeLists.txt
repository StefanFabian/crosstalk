cmake_minimum_required(VERSION 3.10)
project(SerialLibrary)

# Set min C++ standard to 17
if (NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED True)
endif ()

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

set(SOURCES
  include/crosstalk/crosstalker.hpp
  include/crosstalk/serial_abstractions/crosstalk_hardware_serial_wrapper.hpp
  include/crosstalk/refl.hpp
  include/crosstalk/serial_abstraction.hpp
)
add_library(crosstalk INTERFACE)
target_include_directories(crosstalk INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if (BUILD_TESTING)
  enable_testing()
  find_package(GTest REQUIRED)

  find_package(PkgConfig)
  pkg_check_modules(SERIAL libserial)
  if (NOT SERIAL_FOUND)
    message(FATAL_ERROR "libserial not found. Please install it or provide the correct path.")
  endif ()
  add_executable(run_tests test/test_serial_communicator.cpp)
  target_include_directories(run_tests PRIVATE ${SERIAL_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS})
  target_link_libraries(run_tests crosstalk ${GTEST_LIBRARIES} pthread ${SERIAL_LIBRARIES})

  add_executable(test_esp32 test/test_esp32.cpp)
  target_include_directories(test_esp32 PRIVATE ${SERIAL_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS})
  target_link_libraries(test_esp32 crosstalk ${GTEST_LIBRARIES} pthread ${SERIAL_LIBRARIES})
endif ()
