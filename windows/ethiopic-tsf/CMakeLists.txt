cmake_minimum_required(VERSION 3.16)
project(ethiopic-tsf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ethio STATIC
    ../../libethio/src/engine.cpp
    ../../libethio/src/mapping.cpp
    ../../libethio/src/wordlist.cpp
    ../../libethio/src/logger.cpp
)
target_include_directories(ethio PUBLIC
    ../../libethio/include
)
target_compile_features(ethio PUBLIC cxx_std_17)

add_library(ethiopic-tsf SHARED
    dllmain.cpp
    engine.cpp
    ethiopic-tsf.def
)

target_include_directories(ethiopic-tsf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libethio/include
)

target_link_libraries(ethiopic-tsf PRIVATE
    ethio
    ole32
    oleaut32
    advapi32
    uuid
)

target_compile_definitions(ethiopic-tsf PRIVATE
    MAPPING_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../data"
)
