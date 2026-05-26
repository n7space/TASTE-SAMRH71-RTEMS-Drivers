#ifndef SAMRH71_RTEMS_SPW_H
#define SAMRH71_RTEMS_SPW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <Broker.h>

#include <rtems.h>

#include <drivers_config.h>
#include <system_spec.h>
#include <spw.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAMRH71_RTEMS_SPW_RX_PACKET_COUNT 1U
#define SAMRH71_RTEMS_SPW_RX_DATA_SIZE BROKER_BUFFER_SIZE

#ifndef SAMRH71_RTEMS_SPW_TLS_SIZE
#define SAMRH71_RTEMS_SPW_TLS_SIZE 512
#endif

#define SAMRH71_RTEMS_SPW_TASK_STACK_SIZE                          \
	((BROKER_BUFFER_SIZE + 4096U) > RTEMS_MINIMUM_STACK_SIZE ? \
		 (BROKER_BUFFER_SIZE + 4096U) :                    \
		 RTEMS_MINIMUM_STACK_SIZE)
#define SAMRH71_RTEMS_SPW_TASK_BUFFER_SIZE                           \
	(RTEMS_TASK_STORAGE_SIZE(SAMRH71_RTEMS_SPW_TASK_STACK_SIZE + \
					 SAMRH71_RTEMS_SPW_TLS_SIZE, \
				 RTEMS_FLOATING_POINT))

/* --------------------------------------------------------------------------
 * Private data
 * -------------------------------------------------------------------------- */

typedef struct {
	Spw spw;
	enum SystemBus ip_device_bus_id;

	uint8_t link_id;
	uint8_t node_id;
	uint8_t remote_node_id;
	bool remove_prot_id;

	Spw_Rx_RxBufferEntry __attribute__((
		aligned(32))) rx_info[SAMRH71_RTEMS_SPW_RX_PACKET_COUNT];
	uint8_t __attribute__((
		aligned(32))) rx_data[SAMRH71_RTEMS_SPW_RX_DATA_SIZE];

	Spw_Tx_SendListEntry __attribute__((aligned(32))) tx_send_list[1];

	rtems_id tx_semaphore;
	rtems_id rx_semaphore;

	rtems_id task;

	void (*on_tx_timeout)(void *private_data);

	volatile bool rx_deactivated;
	bool init_ok; // True if hardware reset completed within timeout.

	uint8_t task_stack[SAMRH71_RTEMS_SPW_TASK_BUFFER_SIZE];
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

/**
 * @brief Returns whether hardware initialisation completed successfully.
 *
 * @param[in] private_data  Pointer to @ref samrh71_rtems_spw_private_data.
 * @return true if the SPW link left the ErrorReset state within the timeout
 *         during initialisation, false otherwise.
 */
bool samrh71_rtems_spacewire_is_init_ok(const void *private_data);

/**
 * @brief Register a callback invoked when @ref samrh71_rtems_spacewire_send
 *        times out waiting for the previous TX to complete.
 *
 * The callback is called from the send caller's task context with
 * @p private_data as its argument. The timed-out packet is dropped.
 * Pass NULL to disable.
 *
 * @param[in,out] private_data  Pointer to @ref samrh71_rtems_spw_private_data.
 * @param[in]     callback      Function to call on TX timeout, or NULL.
 */
void samrh71_rtems_spacewire_set_tx_timeout_callback(
	void *private_data, void (*callback)(void *private_data));

#ifdef __cplusplus
}
#endif

#endif /* SAMRH71_RTEMS_SPW_H */
