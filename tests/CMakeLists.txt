cmake_minimum_required(VERSION 3.16)
project(ethiopic-keyboard-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(IBUS REQUIRED ibus-1.0>=1.5)
pkg_check_modules(GLIB REQUIRED glib-2.0 gio-2.0)

add_subdirectory(../libethio ${CMAKE_CURRENT_BINARY_DIR}/libethio)

add_executable(test_mapping test_mapping.cpp)
target_link_libraries(test_mapping PRIVATE ethio)
target_compile_definitions(test_mapping PRIVATE
    DATA_DIR="${CMAKE_SOURCE_DIR}/../data"
)

add_executable(test_engine test_engine.cpp)
target_link_libraries(test_engine PRIVATE ethio)
target_compile_definitions(test_engine PRIVATE
    DATA_DIR="${CMAKE_SOURCE_DIR}/../data"
)

add_executable(test_features test_features.cpp)
target_link_libraries(test_features PRIVATE ethio)
target_compile_definitions(test_features PRIVATE
    DATA_DIR="${CMAKE_SOURCE_DIR}/../data"
)

add_executable(test_ibus_engine test_ibus_engine.cpp)
target_include_directories(test_ibus_engine PRIVATE
    ../ibus-ethiopic/src
    ${IBUS_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)
target_link_libraries(test_ibus_engine PRIVATE
    ethio
    ${IBUS_LIBRARIES}
    ${GLIB_LIBRARIES}
)
target_compile_definitions(test_ibus_engine PRIVATE
    DATA_DIR="${CMAKE_SOURCE_DIR}/../data"
    MAPPING_DIR="${CMAKE_SOURCE_DIR}/../data"
    IBUS_ENGINE_NAME="xkb:et:amharic"
    IBUS_COMPONENT_NAME="org.freedesktop.IBus.Ethiopic"
)
target_compile_options(test_ibus_engine PRIVATE
    ${IBUS_CFLAGS_OTHER}
)

# The IBus engine source file must be compiled in for engine type registration
target_sources(test_ibus_engine PRIVATE
    ../ibus-ethiopic/src/engine.cpp
)

enable_testing()
foreach(test_name test_mapping test_engine test_features test_ibus_engine)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/../.."
    )
endforeach()
