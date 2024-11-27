
file(GLOB TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/out_ctrl/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/modbus_client/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/zone_io_client/src/zone_io_client.cpp
)


cmake_minimum_required(VERSION 3.14)
project(unit_tests)

# Global C++20 setting
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Download and build GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Add test executable
add_executable(
    unit_tests
    ${TEST_SOURCES}
)

# Target-specific setting
target_compile_features(unit_tests PRIVATE cxx_std_20)

# Link against GoogleTest and your project libraries
target_link_libraries(
    unit_tests
    GTest::gtest_main
    GTest::gmock_main
)

# Include directories - point to the original component headers
target_include_directories(
    unit_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/helpers/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../components/out_ctrl/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../components/modbus_client/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../components/zone_io_client/include
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(unit_tests)
