add_executable(Samrh71App)
target_sources(Samrh71App
    PRIVATE     main.c)

target_include_directories(Samrh71App
    PRIVATE     ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Samrh71App
    PRIVATE     common_build_options
                SAMV71::Runtime::Hal
                TASTE::SamRH71RtemsSpw
                TASTE::Broker
                SAMV71::RuntimeMocks
                SAMV71::Runtime::BrokerLock)

add_format_target(Samrh71App)
