cmake_minimum_required(VERSION 3.20)
project(polymarket_bot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED CONFIG COMPONENTS system thread)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(poly_bot ${SOURCES}
        include/core/types.hpp
        "include/feed /feed_client.h"
        src/feed/poly_feed.cpp)

target_link_libraries(poly_bot
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        Boost::system
        Boost::thread
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)