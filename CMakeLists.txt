# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
include(${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(test C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(vendor/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src)

# Add executable. Default name is the project name, version 0.1

add_executable(test
        best_store.c
        sd_hw_config.c
        app.c
        launcher.c
        games/games.c
        games/1_push_fast/1_push_fast.c
        games/2_hit20/2_hit20.c
        games/3_TETRIS/3_TETRIS.c
        games/4_ring8/4_ring8.c
        games/5_ninja/5_ninja.c
        games/6_jump/6_jump.c
        games/7_block/7_block.c
        games/8_invader/8_invader.c
        games/9_pinball/9_pinball.c
        bsp/bsp_dma_channel_irq.c
        bsp/bsp_cst816.c
        bsp/bsp_es8311.c
        bsp/bsp_i2c.c
        bsp/bsp_qmi8658.c
        bsp/bsp_st77916.c
        bsp/pio_audio.c
        bsp/pio_qspi.c
)

pico_set_program_name(test "test")
pico_set_program_version(test "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(test 0)
pico_enable_stdio_usb(test 1)

# Add the standard library to the build
target_link_libraries(test
        no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
        pico_stdlib
        hardware_dma
        hardware_i2c
        hardware_irq
        hardware_pio
        hardware_pwm
        hardware_spi)

# Add the standard include files to the build
target_include_directories(test PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/games
  ${CMAKE_CURRENT_LIST_DIR}/bsp
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

pico_generate_pio_header(test ${CMAKE_CURRENT_LIST_DIR}/bsp/pio_qspi.pio)
pico_generate_pio_header(test ${CMAKE_CURRENT_LIST_DIR}/bsp/pio_audio.pio)

pico_add_extra_outputs(test)
