#include "system_spec.h"

#include <drivers_config.h>

enum SystemBus port_to_bus_map[] = {
	BUS_INVALID_ID,
	BUS_BUS_1,
	BUS_BUS_1,
	BUS_BUS_1,
};

enum RemoteInterface bus_to_port_map[] = {
	INTERFACE_INVALID_ID,
	INTERFACE_PINGER_PONG,
	INTERFACE_PINGER_ALIVE,
	INTERFACE_PONGER_PING,
};

struct PartitionBusPair port_to_partition_bus_map[] = {
	{ PARTITION_INVALID_ID, BUS_INVALID_ID },
	{ PARTITION_2, BUS_BUS_1 },
	{ PARTITION_2, BUS_BUS_1 },
	{ PARTITION_1, BUS_BUS_1 },
};

enum SystemBus device_to_bus_map[SYSTEM_DEVICE_NUMBER] = {
	BUS_BUS_1,
	BUS_BUS_1,
};

const void *const device_configurations[SYSTEM_DEVICE_NUMBER] = {
	&Spw_SamRH71_Rtems_Confg_port_1,
	&Spw_SamRH71_Rtems_Confg_port_2,
};

const unsigned packetizer_configurations[SYSTEM_DEVICE_NUMBER] = {
	PACKETIZER_DEFAULT,
	PACKETIZER_DEFAULT,
};
