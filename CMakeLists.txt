cmake_minimum_required(VERSION 3.10)
project(efvi_cpp VERSION 0.1.0 LANGUAGES CXX)

option(EFVI_MOCK_MODE "Build without OpenOnload headers" ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(efvi_cpp
  src/vi_impl.cpp
  src/vi_set_impl.cpp
  src/checksum.cpp
  src/stats.cpp
)

target_include_directories(efvi_cpp PUBLIC include)
if(EFVI_MOCK_MODE)
  target_compile_definitions(efvi_cpp PUBLIC EFVI_MOCK_MODE=1)
endif()

enable_testing()

foreach(t test_init test_config test_log test_tx_rx test_multiqueue)
  add_executable(${t} tests/${t}.cpp)
  target_link_libraries(${t} PRIVATE efvi_cpp)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
