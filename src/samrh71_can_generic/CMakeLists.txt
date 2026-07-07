add_library(SamRH71RtemsCan STATIC)
target_sources(SamRH71RtemsCan
  PRIVATE   samrh71_can_generic.c
  PUBLIC    samrh71_can_generic.h)

 target_include_directories(SamRH71RtemsCan
  PUBLIC    ${CMAKE_SOURCE_DIR}/src/RuntimeMocks
            ${CMAKE_SOURCE_DIR}/TASTE-SAMX-RTEMS-Runtime/src
            ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SamRH71RtemsCan
  PRIVATE   common_build_options
  PUBLIC    SAMRH71::Runtime::Hal
            SAMRH71::Runtime::Core
            TASTE::Broker
            TASTE::Escaper)

add_format_target(SamRH71RtemsCan)

add_library(TASTE::SamRH71RtemsCan ALIAS SamRH71RtemsCan)
