cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/TASTE-SAMX-RTEMS-Runtime/toolchains/samv71.cmake)
project(TASTE-SAMRH71-Drivers VERSION 1.0.0 LANGUAGES C)

add_compile_definitions(N7S_TARGET_SAMRH71F20)
remove_definitions(-DN7S_TARGET_SAMV71Q21)
add_compile_definitions(Pmc_PeripheralId_Hsmci=0)
add_compile_definitions(Pmc_PeripheralId_Spi0=0)
add_compile_definitions(Pmc_PeripheralId_Spi1=0)
add_compile_definitions(Pmc_PeripheralId_Usart0=0)
add_compile_definitions(Pmc_PeripheralId_Usart1=0)
add_compile_definitions(Pmc_PeripheralId_Usart2=0)
add_compile_definitions(Pmc_PeripheralId_Twihs0=0)
add_compile_definitions(Pmc_PeripheralId_Twihs1=0)
add_compile_definitions(Pmc_PeripheralId_Twihs2=0)
add_compile_definitions(Pmc_PeripheralId_Uart0=0)
add_compile_definitions(Pmc_PeripheralId_Uart1=0)
add_compile_definitions(Pmc_PeripheralId_Uart2=0)
add_compile_definitions(Pmc_PeripheralId_Uart3=0)
add_compile_definitions(Pmc_PeripheralId_Uart4=0)
add_compile_definitions(Pmc_PeripheralId_Dacc=0)
add_compile_definitions(Pmc_PeripheralId_Ssc=0)
add_compile_definitions(Pmc_PeripheralId_Afec0=0)
add_compile_definitions(Pmc_PeripheralId_Afec1=0)
add_compile_definitions(Pmc_PeripheralId_Aes=0)
add_compile_definitions(Pmc_MasterckPresc_3=30)
string(REPLACE "-DN7S_TARGET_SAMV71Q21" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/build/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/build/lib)
set(TESTS_OUTPUT_PATH ${CMAKE_BINARY_DIR}/build/tests)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake-tools)

include(MessageColors)
include(LoggingHelper)

add_library(common_build_options INTERFACE)
include(CommonBuildOptions)
include(Sanitizers)

option(TASTE_SAMRH71_DRIVERS_WARNINGS_AS_ERRORS
       "Treat compiler warnings as errors"
       FALSE)

if(TASTE_SAMRH71_DRIVERS_WARNINGS_AS_ERRORS)
    log_option_enabled("warnings as errors")
    set(CLANG_WARNINGS -Werror)
endif()

set(CLANG_WARNINGS ${CLANG_WARNINGS}
                   -Wall
                   -Wextra
                   -Wpedantic
                   -Wshadow
                   -Wcast-align
                   -Wunused
                   -Wsign-conversion
                   -Wnull-dereference
                   -Wdouble-promotion
                   -Wformat=2)

set(GCC_WARNINGS ${CLANG_WARNINGS}
                 -Wimplicit-function-declaration
                 -Wmisleading-indentation
                 -Wduplicated-cond
                 -Wduplicated-branches
                 -Wlogical-op)

log_info(${CMAKE_C_COMPILER_ID})

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(PROJECT_WARNINGS ${CLANG_WARNINGS})
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(PROJECT_WARNINGS ${GCC_WARNINGS})
endif()

target_compile_options(common_build_options INTERFACE ${PROJECT_WARNINGS})

add_library(bsp_build_options INTERFACE)
target_compile_definitions(bsp_build_options
    INTERFACE   USE_UART_IO
                LOW_LEVEL_IO_UART_ID=4)

include(CppCheck)
include(ClangTidy)
include(ClangFormat)
include(Doxygen)

include_directories(SYSTEM
  /opt/rtems/arm-rtems6/atsamv/lib/include
  /opt/rtems/arm-rtems6/include
  TASTE-SAMX-RTEMS-Runtime/src/SamRH71Core)

add_subdirectory(TASTE-Runtime-Common/src/RuntimeMocks)
add_subdirectory(TASTE-Runtime-Common/src/RuntimeCommon)
add_subdirectory(TASTE-Runtime-Common/src/Packetizer)
add_subdirectory(TASTE-Runtime-Common/src/Broker)
add_subdirectory(TASTE-Runtime-Common/src/Escaper)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Startup)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Nvic)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Wdt)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Fpu)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Sdramc)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Scb)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Rstc)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Pio)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Pmc)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Tic)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Uart)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/arm-bsp/src/Utils)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/src/Mocks)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/src/BrokerLock)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/src/SamV71Core)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/src/Hal)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/src/Monitor)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/src/ThreadsCommon)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/src/Xdmac)
add_subdirectory(TASTE-SAMX-RTEMS-Runtime/src/BootHelper)

add_library(SAMRH71::Pmc ALIAS Samv71Pmc)
add_library(SAMRH71::Wdt ALIAS Samv71Wdt)
add_library(SAMRH71::Tic ALIAS Samv71Tic)

add_subdirectory(TASTE-SAMX-RTEMS-Runtime/src/SamRH71Core)

add_subdirectory(src)
