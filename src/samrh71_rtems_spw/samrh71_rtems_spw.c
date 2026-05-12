#include "samrh71_rtems_spw.h"

#include <assert.h>
#include <string.h>

#include <Pmc.h>
#include <Matrix.h>
#include <Nvic.h>
#include <rtems.h>
#include <rtems/irq-extension.h>
#include <Broker.h>

#define SAMRH71_SPW_NVIC_IRQ0 65U
#define SAMRH71_SPW_NVIC_IRQ1 66U

static samrh71_rtems_spw_private_data *g_spw_irq_self = NULL;

static void samrh71_spw_irq_handler(void *arg)
{
	(void)arg;
	if (g_spw_irq_self != NULL) {
		Spw_handleInterrupt(&g_spw_irq_self->spw);
	}
}

#define SPW_INIT_BITRATE_HZ 10000000U
#define RX_DRAIN_TIMEOUT 2000000U

#define SPW_PKTRX_ROUTER_PORT 9U

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
		.rxBufferLength = 1U,
		.rxDataAddress = self->rx_data,
		.rxDataLength = SAMRH71_RTEMS_SPW_RX_DATA_SIZE,
	};
	Spw_Rx_setNextRxBuffer(&self->spw.rx, &rxBufCfg);

	/* Wait for either link to reach Run state AND the buffer to activate.
	 * Checking both links allows the cable to be on Link1 or Link2. */
	Spw_Link_Status linkStatus;
	Spw_Rx_Status rxStatus;
	do {
		Spw_Link_Status ls2;
		Spw_Link_getStatus(&self->spw.link[0], &linkStatus);
		Spw_Link_getStatus(&self->spw.link[1], &ls2);
		if (ls2.linkState == Spw_Link_State_Run) {
			linkStatus = ls2;
		}
		Spw_Rx_getStatus(&self->spw.rx, &rxStatus);
		rtems_task_wake_after(1);
	} while (linkStatus.linkState != Spw_Link_State_Run
		 || !rxStatus.isCurrentReceiveBufferActive);
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

void init_pmc()
{
	Pmc pmc;
	Pmc_init(&pmc, Pmc_getDeviceRegisterStartAddress());

	const Pmc_PeripheralClkConfig spwClk = {
		.isPeripheralClkEnabled = true,
		.isGclkEnabled          = true,
		.gclkSrc                = Pmc_GclkSrc_Mainck,
		.gclkPresc              = 0U,
	};
	Pmc_setPeripheralClkConfig(&pmc, Pmc_PeripheralId_Spw0, &spwClk);
	Pmc_setPeripheralClkConfig(&pmc, Pmc_PeripheralId_Spw1, &spwClk);
}

void init_matrix()
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

  for (uint32_t i = 0; i < 3; i++)
  {
    for (uint32_t j = 0; j < (uint32_t)Matrix_ProtectedRegionId_Count; j++)
    {
      Matrix_setSlaveRegionProtectionConfig(
        &matrix, flexramSlaves[i], (Matrix_ProtectedRegionId)j, &config);
    }
  }
}

void init_nvic_irq(samrh71_rtems_spw_private_data *const self)
{
	g_spw_irq_self = self;

	Nvic_clearInterruptPending(Nvic_Irq_Spw_Irq0);
	Nvic_clearInterruptPending(Nvic_Irq_Spw_Irq1);

	rtems_status_code rc;

	rc = rtems_interrupt_handler_install(
		SAMRH71_SPW_NVIC_IRQ0,
		"SPW0",
		RTEMS_INTERRUPT_SHARED,
		samrh71_spw_irq_handler,
		NULL);
	assert(rc == RTEMS_SUCCESSFUL);

	rc = rtems_interrupt_handler_install(
		SAMRH71_SPW_NVIC_IRQ1,
		"SPW1",
		RTEMS_INTERRUPT_SHARED,
		samrh71_spw_irq_handler,
		NULL);
	assert(rc == RTEMS_SUCCESSFUL);

	Nvic_enableIrq();
}

void init_spw_driver(samrh71_rtems_spw_private_data *const self,
		     const uint8_t txInitDiv, const uint8_t txOperDiv)
{
	init_matrix();
	init_pmc();
	init_nvic_irq(self);

	Spw_init(&self->spw);

	Spw_Link_reset(&self->spw.link[0]);
	Spw_Link_reset(&self->spw.link[1]);
	Spw_Tx_reset(&self->spw.tx);
	Spw_Rx_reset(&self->spw.rx);
	rtems_task_wake_after(5);
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
                .command                 = 3U, // start and listen
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
	Spw_setConfig(&self->spw, &spwCfg);

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
	(void)remote_device_configuration;

	samrh71_rtems_spw_private_data *self =
		(samrh71_rtems_spw_private_data *)private_data;

	assert(self != NULL);
	assert(device_configuration != NULL);

	self->ip_device_bus_id = bus_id;
	self->dest_addr = device_configuration->nodeaddr;
	self->rxblock = true;
	self->txblock = true;
	// TODO add handling of optional configs

	const uint8_t txInitDiv = 0U;
	const uint8_t txOperDiv = 0U;

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

	self->tx_done = false;

	const Spw_Tx_SendListEntryStruct entry = {
		.isEntrySkipped = false,
		.entryType = Spw_Tx_EntryType_PacketData,
		.routerByteLength = 1U,
		.routerByte = { SPW_PKTRX_ROUTER_PORT, 0, 0, 0, 0, 0, 0, 0 },
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
		.routerByte = { self->dest_addr, 0U, 0U, 0U },
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
