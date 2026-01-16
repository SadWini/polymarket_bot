cmake_minimum_required(VERSION 3.10)
project(polymarket_bot)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED CONFIG COMPONENTS system thread)
find_package(nlohmann_json REQUIRED)

add_executable(polymarket_bot main.cpp
        orderbook.h)

target_link_libraries(polymarket_bot
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        Boost::system
        Boost::thread
        nlohmann_json::nlohmann_json
)