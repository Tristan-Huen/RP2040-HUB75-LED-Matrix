cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(hub75matrix C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(hub75matrix 
               src/main.c
               libs/Hub75/hub75.c
)

pico_generate_pio_header(hub75matrix ${CMAKE_SOURCE_DIR}/pio/led_matrix.pio)

target_include_directories(hub75matrix PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/Hub75
)

target_link_libraries(hub75matrix PRIVATE
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_irq
)

target_compile_options(hub75matrix PRIVATE -mcpu=cortex-m0plus)

pico_enable_stdio_usb(hub75matrix 1)
pico_enable_stdio_uart(hub75matrix 0)

pico_add_extra_outputs(hub75matrix)
