cmake_minimum_required(VERSION 3.22)
project(audiophile_native_core_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(audiophile_native_core_tests
    native_core_tests.cpp
    ../pcm_wire_formatter.cpp
    ../usb_playback_state.cpp
    ../dop_formatter.cpp
    ../native_dsd_formatter.cpp
)

target_include_directories(audiophile_native_core_tests PRIVATE .. ../include)
target_compile_options(audiophile_native_core_tests PRIVATE
    -Wall
    -Wextra
    -Werror
    -pedantic
)

option(AUDIPHILE_ENABLE_SANITIZERS "Enable host address/undefined sanitizers" ON)
if(AUDIPHILE_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(audiophile_native_core_tests PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(audiophile_native_core_tests PRIVATE
        -fsanitize=address,undefined
    )
endif()

# Cadence-only host target; stub USB functions never submit a device transfer.
find_package(Threads REQUIRED)
add_executable(audiophile_usb_cadence_tests
    usb_iso_cadence_tests.cpp
    usb_iso_cadence_stubs.cpp
    ../usb_iso_transfer_pool.cpp
    ../spsc_ring_buffer.cpp
    ../uac2_descriptor_parser.cpp
    ../uac2_dsd_detector.cpp
)
target_include_directories(audiophile_usb_cadence_tests PRIVATE include .. ../include)
target_compile_options(audiophile_usb_cadence_tests PRIVATE -Wall -Wextra -Werror)
target_link_libraries(audiophile_usb_cadence_tests PRIVATE Threads::Threads)
if(AUDIPHILE_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(audiophile_usb_cadence_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(audiophile_usb_cadence_tests PRIVATE -fsanitize=address,undefined)
endif()
