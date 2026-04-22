#ifndef SAMRH71_RTEMS_SPACEWIRE_H
#define SAMRH71_RTEMS_SPACEWIRE_H

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

/** Maximum number of packets held in a single RX DMA buffer cycle. */
#define SAMRH71_RTEMS_SPW_RX_PACKET_COUNT   32U

/** Maximum payload size of a single received SpaceWire packet (bytes). */
#define SAMRH71_RTEMS_SPW_RX_MAX_PKT_SIZE   256U

/** Total RX data buffer size: enough for all packets in one cycle. */
#define SAMRH71_RTEMS_SPW_RX_DATA_SIZE \
    (SAMRH71_RTEMS_SPW_RX_PACKET_COUNT * SAMRH71_RTEMS_SPW_RX_MAX_PKT_SIZE)

/** Maximum size of a single TX packet payload passed to Send (bytes). */
#define SAMRH71_RTEMS_SPW_TX_MAX_DATA_SIZE  256U

/** RTEMS task stack size for the poll task. */
#define SAMRH71_RTEMS_SPW_TASK_STACK_SIZE   4096U

/* --------------------------------------------------------------------------
 * Private data
 * -------------------------------------------------------------------------- */

/**
 * @brief Internal state for one SpaceWire driver instance.
 *
 * Allocated by the caller (e.g. TASTE generated glue code) and passed as
 * @p private_data to every driver function.
 */
typedef struct {
    /** SPW wrapper instance. */
    Spw spw;

    /** RTEMS bus-id used when forwarding packets to the Broker. */
    enum SystemBus ip_device_bus_id;

    /** Resolved configuration values. */
    uint8_t  dest_addr;       /**< Destination router byte (nodeaddr from config). */
    bool     remove_prot_id;  /**< Strip first received byte (protocol ID). */
    bool     rxblock;         /**< Block in Poll until natural deactivation. */
    bool     txblock;         /**< Block in Send until TX send list done. */

    /* RX DMA buffers – must be 32-byte aligned for SPW DMA. */
    Spw_Rx_RxBufferEntry  __attribute__((aligned(32))) rx_info[SAMRH71_RTEMS_SPW_RX_PACKET_COUNT];
    uint8_t               __attribute__((aligned(32))) rx_data[SAMRH71_RTEMS_SPW_RX_DATA_SIZE];

    /* TX DMA buffers – single-entry send list, one packet at a time. */
    Spw_Tx_SendListEntry  __attribute__((aligned(32))) tx_send_list[1];
    uint8_t               __attribute__((aligned(32))) tx_data[SAMRH71_RTEMS_SPW_TX_MAX_DATA_SIZE];

    /** Semaphore released by the TX interrupt callback. */
    rtems_id tx_semaphore;
    /** Semaphore released by the RX interrupt callback. */
    rtems_id rx_semaphore;

    /** RTEMS task running Samrh71RtemsSpacewarePoll. */
    rtems_id task;
    /** Stack storage for the poll task. */
    uint8_t  task_stack[SAMRH71_RTEMS_SPW_TASK_STACK_SIZE];

    /** Volatile flags set from interrupt callbacks. */
    volatile bool tx_done;
    volatile bool rx_deactivated;
} samrh71_rtems_spw_private_data;

/* --------------------------------------------------------------------------
 * TASTE driver entry points
 * -------------------------------------------------------------------------- */

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
void Samrh71RtemsSpacewireInit(
    void *private_data,
    enum SystemBus bus_id,
    enum SystemDevice device_id,
    const Spw_SamRH71_Rtems_Conf_T *device_configuration,
    const Spw_SamRH71_Rtems_Conf_T *remote_device_configuration);

/**
 * @brief Blocking receive loop — runs inside a dedicated RTEMS task.
 *
 * Arms the RX DMA buffer, waits for deactivation, extracts packets and
 * forwards each payload to @c Broker_receive_packet.  Loops indefinitely.
 *
 * @param[in,out] private_data  Pointer to @ref Samrh71RtemsSpacewire_PrivateData.
 */
void Samrh71RtemsSpacewarePoll(void *private_data);

/**
 * @brief Transmit @p length bytes of @p data as a single SpaceWire packet.
 *
 * Prepends the configured destination address as the router byte.
 * If @c txblock is set, blocks until the send list deactivates.
 *
 * @param[in,out] private_data  Pointer to @ref Samrh71RtemsSpacewire_PrivateData.
 * @param[in]     data          Payload to transmit.
 * @param[in]     length        Number of bytes to transmit.
 */
void Samrh71RtemsSpacewareSend(
    void *private_data,
    const uint8_t *data,
    size_t length);

#ifdef __cplusplus
}
#endif

#endif /* SAMRH71_RTEMS_SPACEWIRE_H */
