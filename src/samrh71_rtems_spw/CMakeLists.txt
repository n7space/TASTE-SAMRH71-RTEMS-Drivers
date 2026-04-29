set(N7S_SPW_SOURCES
    n7s-spw/src/microchip_spw/plib_spw_link.c
    n7s-spw/src/microchip_spw/plib_spw_pktrx.c
    n7s-spw/src/microchip_spw/plib_spw_pkttx.c
    n7s-spw/src/microchip_spw/plib_spw_router.c
    n7s-spw/src/microchip_spw/plib_spw_tch.c
    n7s-spw/src/link.c
    n7s-spw/src/rx.c
    n7s-spw/src/tx.c
    n7s-spw/src/spw.c
)

add_library(SamRH71RtemsSpw STATIC)
target_sources(SamRH71RtemsSpw
  PRIVATE   samrh71_rtems_spw.c
            ${N7S_SPW_SOURCES}
  PUBLIC    samrh71_rtems_spw.h)

 target_include_directories(SamRH71RtemsSpw
  PUBLIC    ${CMAKE_SOURCE_DIR}/src/RuntimeMocks
            ${CMAKE_SOURCE_DIR}/TASTE-SAMV71-RTEMS-Runtime/src
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/n7s-spw/src
            ${CMAKE_CURRENT_SOURCE_DIR}/n7s-spw/stubs
            ${CMAKE_CURRENT_SOURCE_DIR}/n7s-spw/src/microchip_spw)

target_link_libraries(SamRH71RtemsSpw
  PRIVATE   common_build_options
  PUBLIC    SAMV71::Runtime::Hal
            SAMV71::Runtime::Core
            TASTE::Broker
            TASTE::Escaper)

add_format_target(SamRH71RtemsSpw)

add_library(TASTE::SamRH71RtemsSpw ALIAS SamRH71RtemsSpw)
