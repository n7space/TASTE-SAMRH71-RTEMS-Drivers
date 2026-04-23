#include "samrh71_rtems_spacewire.h"

#include <assert.h>
#include <string.h>

#include <rtems.h>
#include <Broker.h>

Spw g_spw;

/** Default SPW init bit rate target, 10 Mbit/s */
#define SPW_INIT_BITRATE_HZ 10000000U

#define RX_DRAIN_TIMEOUT 2000000U

static void spw_tx_callback(void *arg, const Spw_Tx_IrqStatus *const irqStatus)
{
	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)arg;

	if (irqStatus->sendListDeactivatedIrqOccurred) {
		self->tx_done = true;
		rtems_status_code rc =
			rtems_semaphore_release(self->tx_semaphore);
		(void)rc;
	}
}

static void spw_rx_callback(void *arg, const Spw_Rx_IrqStatus *const irqStatus)
{
	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)arg;

	if (irqStatus->receivedBufferDeactivatedIrqOccurred) {
		self->rx_deactivated = true;
		rtems_status_code rc =
			rtems_semaphore_release(self->rx_semaphore);
		(void)rc;
	}
}

static void arm_rx_buffer(samrh71_rtems_spw_private_data *const self)
{
	self->rx_deactivated = false;

	const Spw_Rx_RxBufferConfig rxBufCfg = {
		.isPacketSplitAndDeactivationEnabled = false,
		.startCondition = Spw_Rx_StartCondition_StartNow,
		.startValue = 0U,
		.rxBufferAddress = self->rx_info,
		.rxBufferLength = SAMRH71_RTEMS_SPW_RX_PACKET_COUNT,
		.rxDataAddress = self->rx_data,
		.rxDataLength = SAMRH71_RTEMS_SPW_RX_DATA_SIZE,
	};
	Spw_Rx_setNextRxBuffer(&self->spw.rx, &rxBufCfg);

	/* Wait until the buffer is active before returning so that TX can be
     * started without risking packets arrive before the RX is armed. */
	Spw_Rx_Status status;
	do {
		Spw_Rx_getStatus(&self->spw.rx, &status);
	} while (!status.isCurrentReceiveBufferActive);
}

static void process_rx_packets(samrh71_rtems_spw_private_data *const self)
{
	Spw_Rx_PreviousRxBufferStatus prevStatus;
	Spw_Rx_getPreviousRxBufferStatus(&self->spw.rx, &prevStatus);

	for (uint16_t i = 0U; i < prevStatus.receivedPackets; i++) {
		Spw_Rx_RxBufferEntryStruct entry;
		Spw_Rx_getRxBufferEntry(&self->rx_info[i], &entry);

		if (!entry.wasEopReceived || entry.wasEepReceived) {
			/* Discard malformed packet. */
			continue;
		}

		const uint8_t *data = entry.dataAddress;
		uint32_t length = entry.dataLength;

		if (self->remove_prot_id) {
			/* Strip the protocol ID byte (first byte). */
			if (length == 0U) {
				continue;
			}
			data++;
			length--;
		}

		if (length > 0U) {
			Broker_receive_packet(self->ip_device_bus_id, data,
					      length);
		}
	}
}

void init_spw_driver(samrh71_rtems_spw_private_data *const self,
		     const uint8_t txInitDiv, const uint8_t txOperDiv)
{
	Spw_init(&g_spw);
	const Spw_Config spwCfg = {
        .link = {
            [0] = {
                .txInitDiv               = txInitDiv,
                .txOperDiv               = txOperDiv,
                .command                 = 3U, // start and listean
                .escChar                 = 0U,
                .escEvent1               = { .active = false, .mask = 0U, .value = 0U },
                .escEvent2               = { .active = false, .mask = 0U, .value = 0U },
                .distributedIrqToEnable  = 0U,
                .distributedIrqToDisable = 0U,
                .distributedAckIrqToEnable  = 0U,
                .distributedAckIrqToDisable = 0U,
            },
            [1] = {
                .txInitDiv               = txInitDiv,
                .txOperDiv               = txOperDiv,
                .command                 = 2U, // start
                .escChar                 = 0U,
                .escEvent1               = { .active = false, .mask = 0U, .value = 0U },
                .escEvent2               = { .active = false, .mask = 0U, .value = 0U },
                .distributedIrqToEnable  = 0U,
                .distributedIrqToDisable = 0U,
                .distributedAckIrqToEnable  = 0U,
                .distributedAckIrqToDisable = 0U,
            },
        },
        .rx = {
            .overrunningPacketPolicy = Spw_Rx_OverrunningPacketPolicy_Stall,
            .irqMaskEnable           = 0U,
            .irqMaskDisable          = 0U,
        },
        .tx = {
            .irqMaskEnable  = 0U,
            .irqMaskDisable = 0U,
        },
    };
	Spw_setConfig(&g_spw, &spwCfg);

	const Spw_TxHandler txHandler = { .callback = spw_tx_callback,
					  .arg = self };
	const Spw_RxHandler rxHandler = { .callback = spw_rx_callback,
					  .arg = self };
	Spw_registerTxHandler(&self->spw, txHandler);
	Spw_registerRxHandler(&self->spw, rxHandler);

	const Spw_Tx_Config txCfg = {
		.irqMaskEnable = (uint32_t)Spw_Tx_Irq_SendListDeactivated,
		.irqMaskDisable = 0U,
	};
	const Spw_Rx_Config rxCfg = {
		.overrunningPacketPolicy = Spw_Rx_OverrunningPacketPolicy_Stall,
		.irqMaskEnable = (uint32_t)Spw_Rx_Irq_ReceivedBufferDeactivated,
		.irqMaskDisable = 0U,
	};
	Spw_Tx_setConfig(&self->spw.tx, &txCfg);
	Spw_Rx_setConfig(&self->spw.rx, &rxCfg);
}

void init_rtems_synchronization_primitives(
	samrh71_rtems_spw_private_data *const self)
{
	self->tx_done = false;
	self->rx_deactivated = false;

	rtems_status_code rc;

	rc = rtems_semaphore_create(rtems_build_name('S', 'P', 'T', 'X'),
				    0U, /* initially locked */
				    RTEMS_SIMPLE_BINARY_SEMAPHORE, 0U,
				    &self->tx_semaphore);
	assert(rc == RTEMS_SUCCESSFUL);

	rc = rtems_semaphore_create(rtems_build_name('S', 'P', 'R', 'X'),
				    0U, /* initially locked */
				    RTEMS_SIMPLE_BINARY_SEMAPHORE, 0U,
				    &self->rx_semaphore);
	assert(rc == RTEMS_SUCCESSFUL);
}

void start_poll_task(samrh71_rtems_spw_private_data *const self)
{
	rtems_status_code rc;

	rtems_task_config taskConfig = {
		.name = rtems_build_name('S', 'P', 'W', 'P'),
		.initial_priority = 1U,
		.storage_area = self->task_stack,
		.storage_size = SAMRH71_RTEMS_SPW_TASK_STACK_SIZE,
		.maximum_thread_local_storage_size = 0U,
		.storage_free = NULL,
		.initial_modes = RTEMS_PREEMPT,
		.attributes = RTEMS_DEFAULT_ATTRIBUTES | RTEMS_FLOATING_POINT,
	};

	rc = rtems_task_construct(&taskConfig, &self->task);
	assert(rc == RTEMS_SUCCESSFUL);

	rc = rtems_task_start(self->task,
			      (rtems_task_entry)Samrh71RtemsSpacewarePoll,
			      (rtems_task_argument)self);
	assert(rc == RTEMS_SUCCESSFUL);
}

void Samrh71RtemsSpacewireInit(
	void *private_data, enum SystemBus bus_id, enum SystemDevice device_id,
	const Spw_SamRH71_Rtems_Conf_T *device_configuration,
	const Spw_SamRH71_Rtems_Conf_T *remote_device_configuration)
{
	(void)device_id;
	(void)remote_device_configuration;

	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)private_data;

	assert(self != NULL);
	assert(device_configuration != NULL);

	self->ip_device_bus_id = bus_id;
	self->dest_addr = device_configuration->nodeaddr;
	// TODO add handling of optional configs

	// Compute TXINITDIV: fall back to TXINITDIV=19, TODO add handling for this divs
	const uint8_t txInitDiv = 19U;
	const uint8_t txOperDiv = 0U; // default max speed

	init_spw_driver(self, txInitDiv, txOperDiv);
	init_rtems_synchronization_primitives(self);
	start_poll_task(self);
}

void Samrh71RtemsSpacewarePoll(void *private_data)
{
	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)private_data;

	while (true) {
		arm_rx_buffer(self);

		if (self->rxblock) {
			/* Blocking mode: wait indefinitely for the RX semaphore released
             * by the ISR when the buffer deactivates. */
			rtems_status_code rc = rtems_semaphore_obtain(
				self->rx_semaphore, RTEMS_WAIT,
				RTEMS_NO_TIMEOUT);
			(void)rc;
		} else {
			/* Non-blocking fallback: spin up to RX_DRAIN_TIMEOUT cycles to
             * let the buffer deactivate naturally.  If it does not, force an
             * abort so we don't stall forever.  Either way we must consume
             * the semaphore token that will be released on deactivation. */
			volatile uint32_t timeout = RX_DRAIN_TIMEOUT;
			while (!self->rx_deactivated && (timeout-- > 0U)) {
			}

			if (!self->rx_deactivated) {
				Spw_Rx_abortOngoingPacketRx(&self->spw.rx);
			}

			/* Wait for the deactivation interrupt (natural or from abort)
             * to keep the semaphore count balanced for the next iteration. */
			rtems_status_code rc = rtems_semaphore_obtain(
				self->rx_semaphore, RTEMS_WAIT,
				RTEMS_NO_TIMEOUT);
			(void)rc;
		}

		process_rx_packets(self);
	}
}

void Samrh71RtemsSpacewareSend(void *private_data, const uint8_t *data,
			       size_t length)
{
	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)private_data;

	assert(data != NULL);
	assert(length > 0U);
	assert(length <= SAMRH71_RTEMS_SPW_TX_MAX_DATA_SIZE);

	memcpy(self->tx_data, data, length);

	self->tx_done = false;

	const Spw_Tx_SendListEntryStruct entry = {
		.isEntrySkipped = false,
		.entryType = Spw_Tx_EntryType_PacketData,
		.routerByteLength = 1U,
		.routerByte = { self->dest_addr, 0, 0, 0, 0, 0, 0, 0 },
		.startTime = 0U,
		.escapeCharMask = 0U,
		.escapeChar = 0U,
		.calculateHeaderCrc = false,
		.headerSize = 0U,
		.headerAddress = NULL,
		.calculateDataCrc = false,
		.dataSize = (uint32_t)length,
		.dataAddress = self->tx_data,
		.timeout = 0U,
	};
	Spw_Tx_setNextSendListEntry(&self->tx_send_list[0], &entry);

	const Spw_Tx_SendListConfig txListCfg = {
		.sendCondition = Spw_Tx_SendCondition_StartNow,
		.sendListLength = 1U,
		.sendListAddress = self->tx_send_list,
		.routerByte = { 0U, 0U, 0U, 0U },
		.abortOngoingSendListWhenStarted = false,
		.startValue = 0U,
	};
	Spw_Tx_setNextSendList(&self->spw.tx, &txListCfg);

	if (self->txblock) {
		rtems_status_code rc = rtems_semaphore_obtain(
			self->tx_semaphore, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
		(void)rc;
	}
}
