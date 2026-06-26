cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_led_effect)

target_sources(app PRIVATE
    src/main.c
    ../../lib/led/led_effect.c
)
target_include_directories(app PRIVATE ../../include)
