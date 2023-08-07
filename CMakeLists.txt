cmake_minimum_required(VERSION 3.0)

project(sosso VERSION 0.1.0)

# Build settings.
include(Build.cmake)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED 20)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${BUILD_COMPILER_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${BUILD_COMPILER_FLAGS}")

find_package(Threads REQUIRED)
find_package(loguru REQUIRED)

message("loguru config found at ${loguru_CONFIG}")
message("fmt config found at ${fmt_CONFIG}")

# Headers.
set(sosso_Headers
  sosso/Buffer.hpp
  sosso/Channel.hpp
  sosso/Correction.hpp
  sosso/Device.hpp
  sosso/DoubleBuffer.hpp
  sosso/FrameClock.hpp
  sosso/Logging.hpp
  sosso/ReadChannel.hpp
  sosso/WriteChannel.hpp
)

# Sources.
set(sosso_Sources
  main.cpp
  TestRun.hpp
)

# Executable
add_executable(sosso
  ${sosso_Headers}
  ${sosso_Sources}
)
target_include_directories(sosso PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(sosso PROPERTIES OUTPUT_NAME "sosso_test")
target_link_libraries(sosso
    PRIVATE Threads::Threads
            loguru::loguru
            execinfo
)

