cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_vbus_debounce)

target_sources(app PRIVATE
    src/main.c
    ../../lib/power/vbus_debounce.c
)
target_include_directories(app PRIVATE ../../include)
