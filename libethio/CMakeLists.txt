cmake_minimum_required(VERSION 3.16)
project(libethio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HEADERS
    include/ethio/mapping.h
    include/ethio/engine.h
    include/ethio/logger.h
    include/ethio/json.hpp
)

set(SOURCES
    src/mapping.cpp
    src/engine.cpp
    src/logger.cpp
)

add_library(ethio STATIC ${SOURCES} ${HEADERS})

target_include_directories(ethio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ethio PUBLIC cxx_std_17)
