add_library(SamV71RtemsSerial STATIC)
target_sources(SamV71RtemsSerial
  PRIVATE   samrh71_rtems_serial.c
  PUBLIC    samrh71_rtems_serial.h
            samrh71_rtems_serial_internal.h)

 target_include_directories(SamV71RtemsSerial
  PUBLIC    ${CMAKE_SOURCE_DIR}/TASTE-SAMX-RTEMS-Runtime/src
            ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SamV71RtemsSerial
  PRIVATE   drivers_common_build_options
  PUBLIC    SAMX71::Runtime::Hal
            SAMX71::Runtime::Core
            TASTE::Broker
            TASTE::Escaper
            SAMRH71::RuntimeMocks)

add_format_target(SamV71RtemsSerial)

add_library(TASTE::SamV71RtemsSerial ALIAS SamV71RtemsSerial)
