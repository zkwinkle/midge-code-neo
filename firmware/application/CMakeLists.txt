cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})



project(blinky)

# Get the current git commit hash
execute_process(
    COMMAND git rev-parse HEAD # replace by hatch version
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Add the commit hash as a compile definition
add_compile_definitions(FW_VERSION="${GIT_COMMIT_HASH}")

target_sources(app PRIVATE
    src/main.c
    src/storage.c
    src/proximity.c
    src/audio.c
    src/battery_charge.c
    src/time_control.c
    src/cmd_processor.c
    src/privacy_switch.c
    src/status_led.c
    )
