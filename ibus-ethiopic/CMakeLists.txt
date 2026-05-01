cmake_minimum_required(VERSION 3.16)
project(ibus-ethiopic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(IBUS REQUIRED ibus-1.0>=1.5)
pkg_check_modules(GLIB REQUIRED glib-2.0 gio-2.0)

if(NOT TARGET ethio)
    add_subdirectory(../libethio ${CMAKE_CURRENT_BINARY_DIR}/libethio)
endif()

set(MAPPING_DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/ibus-ethiopic")
set(MAPPING_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../data")

add_executable(ibus-engine-ethiopic
    src/main.cpp
    src/engine.cpp
)

target_include_directories(ibus-engine-ethiopic PRIVATE
    src
    ${IBUS_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

target_link_libraries(ibus-engine-ethiopic PRIVATE
    ethio
    ${IBUS_LIBRARIES}
    ${GLIB_LIBRARIES}
)

target_compile_definitions(ibus-engine-ethiopic PRIVATE
    MAPPING_DIR="${MAPPING_DATA_DIR}"
    MAPPING_SOURCE_DIR="${MAPPING_SOURCE_DIR}"
)

target_compile_options(ibus-engine-ethiopic PRIVATE
    ${IBUS_CFLAGS_OTHER}
    ${GLIB_CFLAGS_OTHER}
)

install(TARGETS ibus-engine-ethiopic
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}
)

set(LIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}")
configure_file(
    data/org.freedesktop.IBus.Ethiopic.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.IBus.Ethiopic.xml
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.IBus.Ethiopic.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ibus/component
)

install(DIRECTORY ${MAPPING_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ibus-ethiopic
    FILES_MATCHING PATTERN "*.json"
)
