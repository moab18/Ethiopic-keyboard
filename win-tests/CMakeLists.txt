cmake_minimum_required(VERSION 3.16)
project(ethiopic-keyboard-win-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DATA_DIR "${CMAKE_SOURCE_DIR}/../data")
set(LIBETHIO_SRC "${CMAKE_SOURCE_DIR}/../libethio/src")
set(LIBETHIO_INC "${CMAKE_SOURCE_DIR}/../libethio/include")

add_library(ethio STATIC
    ${LIBETHIO_SRC}/engine.cpp
    ${LIBETHIO_SRC}/mapping.cpp
    ${LIBETHIO_SRC}/wordlist.cpp
    ${LIBETHIO_SRC}/logger.cpp
)
target_include_directories(ethio PUBLIC ${LIBETHIO_INC})
target_compile_features(ethio PUBLIC cxx_std_17)

add_executable(test_mapping ../tests/test_mapping.cpp)
target_link_libraries(test_mapping PRIVATE ethio)
target_compile_definitions(test_mapping PRIVATE DATA_DIR="${DATA_DIR}")

add_executable(test_engine ../tests/test_engine.cpp)
target_link_libraries(test_engine PRIVATE ethio)
target_compile_definitions(test_engine PRIVATE DATA_DIR="${DATA_DIR}")

add_executable(test_features ../tests/test_features.cpp)
target_link_libraries(test_features PRIVATE ethio)
target_compile_definitions(test_features PRIVATE DATA_DIR="${DATA_DIR}")

add_executable(test_wordlist ../tests/test_wordlist.cpp)
target_link_libraries(test_wordlist PRIVATE ethio)
target_compile_definitions(test_wordlist PRIVATE DATA_DIR="${DATA_DIR}")

add_executable(test_tsf_engine
    ../tests/test_tsf_engine.cpp
    ../windows/ethiopic-tsf/engine.cpp
)
target_include_directories(test_tsf_engine PRIVATE
    ${LIBETHIO_INC}
    ${CMAKE_SOURCE_DIR}/../windows/ethiopic-tsf
)
target_link_libraries(test_tsf_engine PRIVATE ethio)
target_compile_definitions(test_tsf_engine PRIVATE
    ETHIOPIC_TEST_STUBS
    MAPPING_SOURCE_DIR="${DATA_DIR}"
)

enable_testing()
foreach(test_name test_mapping test_engine test_features test_wordlist test_tsf_engine)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    )
endforeach()
