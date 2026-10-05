#ifndef SYSTEM_SPEC_H
#define SYSTEM_SPEC_H

enum RemoteInterface {
    INTERFACE_INVALID_ID,
    INTERFACE_ACTUATOR_IFACE_A,
    INTERFACE_ACTUATOR_IFACE_B,
    INTERFACE_ACTUATOR_IFACE_C,
    INTERFACE_MAX_ID,
};

enum SystemPartition {
    PARTITION_INVALID_ID,
    PARTITION_1,
    PARTITION_2,
};

enum SystemBus {
    BUS_INVALID_ID,
    BUS_BUS_1,
    BUS_BUS_2,
    BUS_BUS_3,
};

enum PacketizerCfg {
    PACKETIZER_DEFAULT,
    PACKETIZER_CCSDS,
    PACKETIZER_STRICT_CCSDS,
    PACKETIZER_THIN,
    PACKETIZER_DEVICE_PROVIDED,
    PACKETIZER_PASSTHROUGH,
    PACKETIZER_MAX_ID,
};

#define SYSTEM_BUSES_NUMBER (3u + 1u)

struct PartitionBusPair
{
    enum SystemPartition partition;
    enum SystemBus bus;
};

extern enum SystemBus port_to_bus_map[];
extern enum RemoteInterface bus_to_port_map[];
extern struct PartitionBusPair port_to_partition_bus_map[];

enum SystemDevice
{
    DEVICE_NODE_1_SPW0,
    DEVICE_NODE_1_UART0,
    DEVICE_NODE_1_CAN0,
    DEVICE_NODE_2_SPW0,
    DEVICE_NODE_2_UART0,
    DEVICE_NODE_2_CAN0,
    DEVICE_INVALID_ID,
};

#define SYSTEM_DEVICE_NUMBER (6u + 1u)

extern enum SystemBus device_to_bus_map[SYSTEM_DEVICE_NUMBER];
extern const void* const device_configurations[SYSTEM_DEVICE_NUMBER];
extern const unsigned packetizer_configurations[SYSTEM_DEVICE_NUMBER];
extern int bus_message_size[SYSTEM_BUSES_NUMBER];

#ifdef __cplusplus
extern "C" {
#endif
void initialize_system_spec();
#ifdef __cplusplus
}
#endif
#endif
