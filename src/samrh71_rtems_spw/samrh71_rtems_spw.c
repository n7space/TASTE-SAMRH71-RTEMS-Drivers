#include "samrh71_rtems_spw.h"

#include <assert.h>
#include <string.h>

#include <SamRH71Core.h>
#include <Pmc.h>
#include <Matrix.h>
#include <Nvic.h>
#include <rtems.h>
#include <rtems/irq-extension.h>
#include <Broker.h>
#include <router.h>

#define SAMRH71_SPW_NVIC_IRQ0 65U
#define SAMRH71_SPW_NVIC_IRQ1 66U
#define SPW_PKTRX_ROUTER_PORT 9U

#define SPW_PKTRX_BUFFER_LENGTH 1U

#define SPW_LINK_START_AND_LISTEN_COMMAND 3U

#define SPW_RESET_TIMEOUT_TICKS 100U

#define MEGA_HZ 1000000u

static samrh71_rtems_spw_private_data *g_spw_irq_self = NULL;
static uint64_t master_clock_frequency = 0;

static void samrh71_spw_irq_handler(void *arg)
{
	(void)arg;
	if (g_spw_irq_self != NULL) {
		Spw_handleInterrupt(&g_spw_irq_self->spw);
	}
}

static void spw_tx_callback(void *arg, const Spw_Tx_IrqStatus *const irqStatus)
{
	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)arg;

	if (irqStatus->sendListDeactivatedIrqOccurred) {
		Spw_Tx_Status status;
		Spw_Tx_getStatus(&self->spw.tx, &status);

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
		.rxBufferLength = SPW_PKTRX_BUFFER_LENGTH,
		.rxDataAddress = self->rx_data,
		.rxDataLength = SAMRH71_RTEMS_SPW_RX_DATA_SIZE,
	};
	Spw_Rx_setNextRxBuffer(&self->spw.rx, &rxBufCfg);

	Spw_Rx_Status rxStatus;
	do {
		Spw_Rx_getStatus(&self->spw.rx, &rxStatus);
	} while (!rxStatus.isCurrentReceiveBufferActive);
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

		uint8_t *data = entry.dataAddress;
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

static void init_pmc(const samrh71_rtems_spw_private_data *const self)
{
	Pmc pmc;
	Pmc_init(&pmc, Pmc_getDeviceRegisterStartAddress());

	const Pmc_PeripheralClkConfig spwClk = {
		.isPeripheralClkEnabled = true,
		.isGclkEnabled = true,
		.gclkSrc = Pmc_GclkSrc_Masterck,
		.gclkPresc = 0U, // this is a divider, leave it to zero
	};

	// Main spw peripheral clock, always active
	Pmc_setPeripheralClkConfig(&pmc, Pmc_PeripheralId_Spw0, &spwClk);

	// Configure optional spw peripheral clock for Link 2
	if (self->link_id == 2U) {
		Pmc_setPeripheralClkConfig(&pmc, Pmc_PeripheralId_Spw1,
					   &spwClk);
	}
}

static void init_matrix()
{
	// Configure Matrix
	// Workaround for Hardware bug related to memory access on SAMRH71F20
	// impacts peripherals using DMA: Mcan, Xdmac, Gmac and SpaceWire
	// details in document DS80000875D - Rad-Hard 32-bit Arm Cortex-M7 Microcontroller
	// for Aerospace Applications Errata Sheet
	// Erratum number 11
	Matrix matrix;
	Matrix_init(&matrix, Matrix_getDeviceBaseAddress());

	const Matrix_Slave flexramSlaves[] = {
		Matrix_Slave_Flexram0,
		Matrix_Slave_Flexram1,
		Matrix_Slave_Flexram2,
	};
	const Matrix_SlaveRegionProtectionConfig config = {
		.isPrivilegedRegionUserWriteAllowed = true,
		.isPrivilegedRegionUserReadAllowed = true,
		.regionSplitOffset = Matrix_Size_128MB,
		.regionOrder = Matrix_RegionSplitOrder_UpperPrivilegedLowerUser,
	};

	for (uint32_t i = 0; i < 3; i++) {
		for (uint32_t j = 0;
		     j < (uint32_t)Matrix_ProtectedRegionId_Count; j++) {
			Matrix_setSlaveRegionProtectionConfig(
				&matrix, flexramSlaves[i],
				(Matrix_ProtectedRegionId)j, &config);
		}
	}
}

static void init_nvic_irq(samrh71_rtems_spw_private_data *const self)
{
	g_spw_irq_self = self;

	Nvic_clearInterruptPending(Nvic_Irq_Spw_Irq0);
	Nvic_clearInterruptPending(Nvic_Irq_Spw_Irq1);

	rtems_status_code rc;

	rc = rtems_interrupt_handler_install(SAMRH71_SPW_NVIC_IRQ0, "SPW0",
					     RTEMS_INTERRUPT_SHARED,
					     samrh71_spw_irq_handler, NULL);
	assert(rc == RTEMS_SUCCESSFUL);

	rc = rtems_interrupt_handler_install(SAMRH71_SPW_NVIC_IRQ1, "SPW1",
					     RTEMS_INTERRUPT_SHARED,
					     samrh71_spw_irq_handler, NULL);
	assert(rc == RTEMS_SUCCESSFUL);

	Nvic_enableIrq();
}

static void init_spw_router(const samrh71_rtems_spw_private_data *const self)
{
	Spw_Router_init();

	Spw_Router_Config config;
	config.isTimeoutDisabled = true;
	config.isFallbackRoutingEnabled = false;
	config.isLogicalAddressRoutingEnabled = true;

	Spw_Router_setConfig(&config);

	Spw_Router_TableEntry router_entry;
	router_entry.deleteHeaderByte = true;
	router_entry.address = SPW_PKTRX_ROUTER_PORT;

	Spw_Router_setTableEntry(self->node_id, &router_entry);

	router_entry.deleteHeaderByte = false;
	router_entry.address = self->link_id;

	Spw_Router_setTableEntry(self->remote_node_id, &router_entry);
}

static void check_spw_is_init_ok(samrh71_rtems_spw_private_data *const self)
{
	// After applying command=3 the link transitions:
	// ErrorReset -> ErrorWait -> Ready
	Spw_Link_Status linkStatus;
	rtems_interval ticks = 0U;
	do {
		rtems_task_wake_after(1);
		ticks++;
		Spw_Link_getStatus(&self->spw.link[self->link_id - 1U],
				   &linkStatus);
	} while (linkStatus.linkState < Spw_Link_State_Ready &&
		 ticks < SPW_RESET_TIMEOUT_TICKS);

	self->init_ok = (linkStatus.linkState >= Spw_Link_State_Ready);
}

static void init_spw_driver(samrh71_rtems_spw_private_data *const self,
			    const uint8_t txInitDiv, const uint8_t txOperDiv)
{
	init_matrix();
	init_pmc(self);
	init_nvic_irq(self);

	Spw_init(&self->spw);
	init_spw_router(self);

	Spw_Link_reset(&self->spw.link[0]);
	Spw_Link_reset(&self->spw.link[1]);
	Spw_Tx_reset(&self->spw.tx);
	Spw_Rx_reset(&self->spw.rx);

	// Only the configured link is started (command=3).
	// The other link is kept disabled (command=0).
	const Spw_Link_Config activeLinkCfg = {
		.txInitDiv = txInitDiv,
		.txOperDiv = txOperDiv,
		.command = SPW_LINK_START_AND_LISTEN_COMMAND,
		.escChar = 0U,
		.escEvent1 = { .active = false, .mask = 0U, .value = 0U },
		.escEvent2 = { .active = false, .mask = 0U, .value = 0U },
		.distributedIrqToEnable = 0U,
		.distributedIrqToDisable = 0U,
		.distributedAckIrqToEnable = 0U,
		.distributedAckIrqToDisable = 0U,
	};
	const Spw_Link_Config disabledLinkCfg = {
		.txInitDiv = txInitDiv,
		.txOperDiv = txOperDiv,
		.command = 0U,
		.escChar = 0U,
		.escEvent1 = { .active = false, .mask = 0U, .value = 0U },
		.escEvent2 = { .active = false, .mask = 0U, .value = 0U },
		.distributedIrqToEnable = 0U,
		.distributedIrqToDisable = 0U,
		.distributedAckIrqToEnable = 0U,
		.distributedAckIrqToDisable = 0U,
	};
	const Spw_Config spwCfg = {
		.link = {
			[0] = (self->link_id == 1U) ? activeLinkCfg : disabledLinkCfg,
			[1] = (self->link_id == 2U) ? activeLinkCfg : disabledLinkCfg,
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
	Spw_setConfig(&self->spw, &spwCfg);
	check_spw_is_init_ok(self);

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

static void init_rtems_synchronization_primitives(
	samrh71_rtems_spw_private_data *const self)
{
	self->rx_deactivated = false;
	self->on_tx_timeout = NULL;

	rtems_status_code rc;

	rc = rtems_semaphore_create(rtems_build_name('S', 'P', 'T', 'X'),
				    1U, /* initially unlocked */
				    RTEMS_SIMPLE_BINARY_SEMAPHORE, 0U,
				    &self->tx_semaphore);
	assert(rc == RTEMS_SUCCESSFUL);

	rc = rtems_semaphore_create(rtems_build_name('S', 'P', 'R', 'X'),
				    0U, /* initially locked */
				    RTEMS_SIMPLE_BINARY_SEMAPHORE, 0U,
				    &self->rx_semaphore);
	assert(rc == RTEMS_SUCCESSFUL);
}

static void start_poll_task(samrh71_rtems_spw_private_data *const self)
{
	rtems_status_code rc;

	rtems_task_config taskConfig = {
		.name = rtems_build_name('S', 'P', 'W', 'P'),
		.initial_priority = 1U,
		.storage_area = self->task_stack,
		.storage_size = SAMRH71_RTEMS_SPW_TASK_BUFFER_SIZE,
		.maximum_thread_local_storage_size = SAMRH71_RTEMS_SPW_TLS_SIZE,
		.storage_free = NULL,
		.initial_modes = RTEMS_PREEMPT,
		.attributes = RTEMS_DEFAULT_ATTRIBUTES | RTEMS_FLOATING_POINT,
	};

	rc = rtems_task_construct(&taskConfig, &self->task);
	assert(rc == RTEMS_SUCCESSFUL);

	rc = rtems_task_start(self->task,
			      (rtems_task_entry)samrh71_rtems_spacewire_poll,
			      (rtems_task_argument)self);
	assert(rc == RTEMS_SUCCESSFUL);
}

void samrh71_rtems_spacewire_init(
	void *private_data, enum SystemBus bus_id, enum SystemDevice device_id,
	const Spw_SamRH71_Rtems_Conf_T *device_configuration,
	const Spw_SamRH71_Rtems_Conf_T *remote_device_configuration)
{
	(void)device_id;

	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)private_data;

	assert(self != NULL);
	assert(device_configuration != NULL);

	self->ip_device_bus_id = bus_id;
	self->link_id = (uint8_t)device_configuration->link_id + 1U;
	self->node_id = device_configuration->node_id;
	self->remote_node_id = remote_device_configuration->node_id;
	self->remove_prot_id =
		device_configuration->exist.remove_prot_id ?
			(bool)device_configuration->remove_prot_id :
			false;

	master_clock_frequency = SamRH71Core_GetMainClockFrequency();

	// SpaceWire standard requires init rate 10 Mbit/s.
	const uint64_t init_bitrate = 10U * MEGA_HZ;
	assert(init_bitrate <= 2U * master_clock_frequency);
	const uint8_t txInitDiv =
		(uint8_t)(((2U * master_clock_frequency + init_bitrate - 1U) /
			   init_bitrate) -
			  1U);

	// default operational speed: same as init (10 Mbit/s)
	uint8_t txOperDiv = txInitDiv;
	if (device_configuration->exist.link_speed &&
	    device_configuration->link_speed > 0U) {
		const uint64_t oper_bitrate =
			(uint64_t)device_configuration->link_speed * MEGA_HZ;
		assert(oper_bitrate <= 2U * master_clock_frequency);
		txOperDiv = (uint8_t)(((2U * master_clock_frequency +
					oper_bitrate - 1U) /
				       oper_bitrate) -
				      1U);
	}

	init_spw_driver(self, txInitDiv, txOperDiv);
	init_rtems_synchronization_primitives(self);
	start_poll_task(self);
}

static void wait_for_rx_deactivation(samrh71_rtems_spw_private_data *const self)
{
	rtems_status_code rc = rtems_semaphore_obtain(
		self->rx_semaphore, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
	(void)rc;
}

void samrh71_rtems_spacewire_poll(void *private_data)
{
	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)private_data;

	while (true) {
		arm_rx_buffer(self);
		wait_for_rx_deactivation(self);
		process_rx_packets(self);
	}
}

void samrh71_rtems_spacewire_send(void *private_data, const uint8_t *data,
				  size_t length)
{
	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)private_data;

	assert(data != NULL);
	assert(length > 0U);

	rtems_status_code rc =
		rtems_semaphore_obtain(self->tx_semaphore, RTEMS_WAIT,
				       SAMRH71_RTEMS_SPW_TX_TIMEOUT_TICKS);

	if (rc == RTEMS_TIMEOUT) {
		if (self->on_tx_timeout != NULL) {
			self->on_tx_timeout(self);
		}
		return;
	}

	const Spw_Tx_SendListEntryStruct entry = {
		.isEntrySkipped = false,
		.entryType = Spw_Tx_EntryType_PacketData,
		.routerByteLength = 0U,
		.routerByte = { 0, 0, 0, 0, 0, 0, 0, 0 },
		.startTime = 0U,
		.escapeCharMask = 0U,
		.escapeChar = 0U,
		.calculateHeaderCrc = false,
		.headerSize = 0U,
		.headerAddress = NULL,
		.calculateDataCrc = false,
		.dataSize = (uint32_t)length,
		.dataAddress = (uint8_t *)(uintptr_t)data,
		.timeout = 0U,
	};
	Spw_Tx_setNextSendListEntry(&self->tx_send_list[0], &entry);

	const Spw_Tx_SendListConfig txListCfg = {
		.sendCondition = Spw_Tx_SendCondition_StartNow,
		.sendListLength = 1U,
		.sendListAddress = self->tx_send_list,
		.routerByte = { self->remote_node_id, 0U, 0U, 0U },
		.abortOngoingSendListWhenStarted = false,
		.startValue = 0U,
	};
	Spw_Tx_setNextSendList(&self->spw.tx, &txListCfg);
}

bool samrh71_rtems_spacewire_is_init_ok(const void *private_data)
{
	const samrh71_rtems_spw_private_data *self =
		(const samrh71_rtems_spw_private_data *)private_data;
	return self->init_ok;
}

void samrh71_rtems_spacewire_set_tx_timeout_callback(
	void *private_data, void (*callback)(void *private_data))
{
	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)private_data;
	self->on_tx_timeout = callback;
}
