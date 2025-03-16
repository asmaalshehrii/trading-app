cmake_minimum_required(VERSION 3.10)
project(trading_app)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(/opt/homebrew/include)  # for asio

add_executable(trading_app main.cpp)
target_link_libraries(trading_app pthread)
