# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")
set(PICO_PLATFORM rp2350)

# FreeRTOS-Kernel vendorizado na raiz do projeto
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Kernel)

cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

project(pico_emb C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(PICO_SDK_VERSION_STRING VERSION_LESS "2.2.0")
  message(FATAL_ERROR
    "Raspberry Pi Pico SDK version 2.2.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Initialize the SDK
pico_sdk_init()

# FreeRTOS (porta RP2350 ARM, single-core -> ver configNUMBER_OF_CORES abaixo)
include(FreeRTOS_Kernel_import.cmake)

add_compile_options(
  -Wall
  -Wno-format          # int != int32_t as far as the compiler is concerned
  -Wno-unused-function # some helpers are not always called
)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wno-maybe-uninitialized)
endif()

add_executable(pico_emb
  src/main.c
  src/drivers/mpu6050.c
  src/drivers/hcsr04.c
  src/drivers/power_sense.c
  src/drivers/actuator.c
  src/drivers/hc06.c
  src/tasks/control_task.c
  src/tasks/imu_task.c
  src/tasks/sensor_distance_task.c
  src/tasks/power_task.c
  src/tasks/command_task.c
  src/tasks/telemetry_task.c
)

target_include_directories(pico_emb PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}          # FreeRTOSConfig.h
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/src/drivers
  ${CMAKE_CURRENT_LIST_DIR}/src/tasks
)

# Debug (printf) sai pela USB; a UART fica livre para o modulo HC-05
pico_enable_stdio_usb(pico_emb 1)
pico_enable_stdio_uart(pico_emb 0)

target_link_libraries(pico_emb
  pico_stdlib
  FreeRTOS-Kernel-Heap4
  hardware_i2c
  hardware_adc
  hardware_gpio
  hardware_uart
  hardware_pwm
  hardware_timer
)

# Single-core (default do port SMP e 2). Ver plano: multicore nao e necessario.
target_compile_definitions(pico_emb PRIVATE
  configNUMBER_OF_CORES=1
)

set_target_properties(pico_emb PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(pico_emb)
