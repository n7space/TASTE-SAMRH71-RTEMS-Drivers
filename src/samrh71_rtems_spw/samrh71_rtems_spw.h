#ifndef SAMRH71_RTEMS_SPW_H
#define SAMRH71_RTEMS_SPW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <rtems.h>

#include <drivers_config.h>
#include <system_spec.h>
#include <spw.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAMRH71_RTEMS_SPW_RX_PACKET_COUNT 1U

#ifndef SAMRH71_RTEMS_SPW_RX_DATA_SIZE
#define SAMRH71_RTEMS_SPW_RX_DATA_SIZE 1024U
#endif


#ifndef SAMRH71_RTEMS_SPW_TLS_SIZE
#define SAMRH71_RTEMS_SPW_TLS_SIZE 512
#endif

#define SAMRH71_RTEMS_SPW_TASK_STACK_SIZE \
	(8192 > RTEMS_MINIMUM_STACK_SIZE ? 8192 : RTEMS_MINIMUM_STACK_SIZE)
#define SAMRH71_RTEMS_SPW_TASK_BUFFER_SIZE                                \
	(RTEMS_TASK_STORAGE_SIZE(SAMRH71_RTEMS_SPW_TASK_STACK_SIZE +           \
					 SAMRH71_RTEMS_SPW_TLS_SIZE, \
				 RTEMS_FLOATING_POINT))

/* --------------------------------------------------------------------------
 * Private data
 * -------------------------------------------------------------------------- */

typedef struct {
	Spw spw;
	enum SystemBus ip_device_bus_id;

	uint8_t dest_addr;
	bool remove_prot_id;
	bool rxblock;
	bool txblock;

	Spw_Rx_RxBufferEntry __attribute__((aligned(32))) rx_info[SAMRH71_RTEMS_SPW_RX_PACKET_COUNT];
	uint8_t __attribute__((aligned(32))) rx_data[SAMRH71_RTEMS_SPW_RX_DATA_SIZE];

	Spw_Tx_SendListEntry __attribute__((aligned(32))) tx_send_list[1];

	rtems_id tx_semaphore;
	rtems_id rx_semaphore;

	rtems_id task;
	uint8_t task_stack[SAMRH71_RTEMS_SPW_TASK_BUFFER_SIZE];

	volatile bool tx_done;
	volatile bool rx_deactivated;
} samrh71_rtems_spw_private_data;

/**
 * @brief Initialize the samrh71_rtems_spw.
 *
 * Function is used by runtime to initialize the driver.
 *
 * @param[in,out] private_data  Pointer to a caller-allocated
 *                              @ref samrh71_rtems_spw_private_data.
 * @param[in]     bus_id        TASTE system bus identifier for Broker calls.
 * @param[in]     device_id     TASTE system device identifier (unused).
 * @param[in]     device_configuration     Local device configuration.
 * @param[in]     remote_device_configuration  Remote peer configuration (unused).
 */
void samrh71_rtems_spacewire_init(
	void *private_data, enum SystemBus bus_id, enum SystemDevice device_id,
	const Spw_SamRH71_Rtems_Conf_T *device_configuration,
	const Spw_SamRH71_Rtems_Conf_T *remote_device_configuration);

/**
 * @brief Blocking receive loop — runs inside a dedicated RTEMS task.
 *
 * Arms the RX DMA buffer, waits for deactivation, extracts packets and
 * forwards each payload to Broker.  Loops indefinitely.
 *
 * @param[in,out] private_data  Pointer to @ref samrh71_rtems_spw_private_data.
 */
void samrh71_rtems_spacewire_poll(void *private_data);

/**
 * @brief Transmit @p length bytes of @p data as a single SpaceWire packet.
 *
 * @param[in,out] private_data  Pointer to @ref samrh71_rtems_spw_private_data.
 * @param[in]     data          Payload to transmit.
 * @param[in]     length        Number of bytes to transmit.
 */
void samrh71_rtems_spacewire_send(void *private_data, const uint8_t *data,
				  size_t length);

#ifdef __cplusplus
}
#endif

#endif /* SAMRH71_RTEMS_SPW_H */
