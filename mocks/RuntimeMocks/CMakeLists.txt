add_library(SamRH71RuntimeMocks STATIC)
target_sources(SamRH71RuntimeMocks
  PRIVATE   asn1crt.c
            drivers_config.c
            interfaces_info.c
            samrh71-rtems-spw-driver.c
            system_spec.c
  PUBLIC    asn1crt.h
            drivers_config.h
            interfaces_info.h
            samrh71-rtems-spw-driver.h
            system_spec.h)

target_include_directories(SamRH71RuntimeMocks
  PUBLIC    ${CMAKE_SOURCE_DIR}/TASTE-SAMX-RTEMS-Runtime/src
            ${CMAKE_CURRENT_SOURCE_DIR})

add_format_target(SamRH71RuntimeMocks)

add_library(SAMRH71::RuntimeMocks ALIAS SamRH71RuntimeMocks)
