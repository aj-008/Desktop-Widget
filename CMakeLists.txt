cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(display C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()


add_executable(widget
    src/main.c
    src/clock.c
    src/quote.c
    src/mandelbrot.c
    src/ball.c

    lib/src/ST7789/hardware_init.c
    lib/src/graphics/util.c
    lib/src/graphics/text.c
    lib/src/graphics/image.c
    lib/src/graphics/shapes.c
)

target_include_directories(widget PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(widget 
    pico_stdlib
    pico_rand
    hardware_spi
    hardware_dma
    hardware_rtc
)

pico_enable_stdio_usb(widget 1)
pico_enable_stdio_uart(widget 0)

pico_add_extra_outputs(widget)
