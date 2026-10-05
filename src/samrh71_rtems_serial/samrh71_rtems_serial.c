/**@file
 * This file is part of the TASTE SAMRH71 RTEMS Drivers.
 *
 * @copyright 2026 N7 Space Sp. z o.o.
 *
 * Licensed under the ESA Public License (ESA-PL) Permissive (Type 3),
 * Version 2.4 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://essr.esa.int/license/list
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "samrh71_rtems_serial.h"

#include <Broker.h>
#include <rtems.h>
#include <assert.h>
#include <stdlib.h>

#include <Hal.h>

#include <Escaper.h>
#include <EscaperInternal.h>
#include <Nvic/Nvic.h>
#include <Uart/Uart.h>
#include <Scb/Scb.h>
#include <Pio/samrh/Pio.h>
#include <Nvic/Nvic.h>
#include <Pmc/Pmc.h>
#include <SamRH71Core/SamRH71Core.h>

static Samrh71RtemsSerial_UserUartErrorCallback
	Samrh71RtemsSerial_user_uart_error_callback = NULL;
static void *Samrh71RtemsSerial_user_uart_error_callback_arg = NULL;

static Uart *uart0handle;
static Uart *uart1handle;
static Uart *uart2handle;
static Uart *uart3handle;
static Uart *uart4handle;
static Uart *uart5handle;
static Uart *uart6handle;
static Uart *uart7handle;
static Uart *uart8handle;
static Uart *uart9handle;

/**
 * @brief UART priority definition - !! IMPORTANT !!
 * Interrupts that use RTEMS functions must have smaller priorities than
 * kernel interrupts levels. The lower the priority value, the
 * higher the priority is. In RTEMS on Cortex-M7, the PRIMASK in critical
 * sections is set to 0x80 - therefore, in order to avoid a
 * race condition (for example, an IRQ preempting internal RTEMS processing
 * and modifying it's internal data structures in a way unexpected by RTEMS,
 * potentially causing an undefined behaviour), IRQ handlers that use
 * RTEMS-related functionality like semaphores, events, etc. **MUST HAVE THEIR
 * PRIORITY SET TO AT LEAST 0x80 (4 after the bit-shift), OR LOWER (so,
 * higher value)**.
 * Safe IRQ priorities are in 4-7 inclusive range on SAMV71 and SAMRH71,
 * which corresponds to values 0x80, 0xA0, 0xC0 and 0xE0 for the
 * rtems_interrupt_set_priority function, as it expects shifted
 * values and writes them directly to NVIC register.
 */
#define UART_INTERRUPT_PRIORITY 4

#define UART_RX_EVENT RTEMS_EVENT_0

#ifndef UART_BLOCKING_WRITE_TIMEOUT
#define UART_BLOCKING_WRITE_TIMEOUT 10000
#endif

void UART0_Handler(void)
{
	ErrorCode errCode = 0;
	if (uart0handle != NULL)
		Uart_handleInterrupt(uart0handle, &errCode);
}

void UART1_Handler(void)
{
	ErrorCode errCode = 0;
	if (uart1handle != NULL)
		Uart_handleInterrupt(uart1handle, &errCode);
}

void UART2_Handler(void)
{
	ErrorCode errCode = 0;
	if (uart2handle != NULL)
		Uart_handleInterrupt(uart2handle, &errCode);
}

void UART3_Handler(void)
{
	ErrorCode errCode = 0;
	if (uart3handle != NULL)
		Uart_handleInterrupt(uart3handle, &errCode);
}

void UART4_Handler(void)
{
	ErrorCode errCode = 0;
	if (uart4handle != NULL)
		Uart_handleInterrupt(uart4handle, &errCode);
}

void UART5_Handler(void)
{
	ErrorCode errCode = 0;
	if (uart5handle != NULL)
		Uart_handleInterrupt(uart5handle, &errCode);
}

void UART6_Handler(void)
{
	ErrorCode errCode = 0;
	if (uart6handle != NULL)
		Uart_handleInterrupt(uart6handle, &errCode);
}

void UART7_Handler(void)
{
	ErrorCode errCode = 0;
	if (uart7handle != NULL)
		Uart_handleInterrupt(uart7handle, &errCode);
}

void UART8_Handler(void)
{
	ErrorCode errCode = 0;
	if (uart8handle != NULL)
		Uart_handleInterrupt(uart8handle, &errCode);
}

void UART9_Handler(void)
{
	ErrorCode errCode = 0;
	if (uart9handle != NULL)
		Uart_handleInterrupt(uart9handle, &errCode);
}

/// \brief Flexcom device identifiers.
typedef enum {
	Flexcom_Id_0 = 0, ///< Flexcom device 0.
	Flexcom_Id_1 = 1, ///< Flexcom device 1.
	Flexcom_Id_2 = 2, ///< Flexcom device 2.
	Flexcom_Id_3 = 3, ///< Flexcom device 3.
	Flexcom_Id_4 = 4, ///< Flexcom device 4.
	Flexcom_Id_5 = 5, ///< Flexcom device 5.
	Flexcom_Id_6 = 6, ///< Flexcom device 6.
	Flexcom_Id_7 = 7, ///< Flexcom device 7.
	Flexcom_Id_8 = 8, ///< Flexcom device 8.
	Flexcom_Id_9 = 9, ///< Flexcom device 9.
	Flexcom_Id_Count = 10, ///< Number of available instances of Flexcom.
} Flexcom_Id;

static void initUartIrq(const Nvic_Irq irqn, const char *handlerName,
			rtems_interrupt_handler handler)
{
	Nvic_clearInterruptPending(irqn);
	Nvic_setInterruptPriority(irqn, UART_INTERRUPT_PRIORITY);
	SamRH71Core_InterruptSubscribe(irqn, handlerName, handler, NULL);
}

static void uartLowLevelInit()
{
	static bool SamRH71RtemsSerial_inited = false;
	if (!SamRH71RtemsSerial_inited) {
		SamRH71RtemsSerial_inited = true;
		initUartIrq(Nvic_Irq_Flexcom0, "uart0",
			    (rtems_interrupt_handler)&UART0_Handler);
		initUartIrq(Nvic_Irq_Flexcom1, "uart1",
			    (rtems_interrupt_handler)&UART1_Handler);
		initUartIrq(Nvic_Irq_Flexcom2, "uart2",
			    (rtems_interrupt_handler)&UART2_Handler);
		initUartIrq(Nvic_Irq_Flexcom3, "uart3",
			    (rtems_interrupt_handler)&UART3_Handler);
		initUartIrq(Nvic_Irq_Flexcom4, "uart4",
			    (rtems_interrupt_handler)&UART4_Handler);
		initUartIrq(Nvic_Irq_Flexcom5, "uart5",
			    (rtems_interrupt_handler)&UART5_Handler);
		initUartIrq(Nvic_Irq_Flexcom6, "uart6",
			    (rtems_interrupt_handler)&UART6_Handler);
		initUartIrq(Nvic_Irq_Flexcom7, "uart7",
			    (rtems_interrupt_handler)&UART7_Handler);
		initUartIrq(Nvic_Irq_Flexcom8, "uart8",
			    (rtems_interrupt_handler)&UART8_Handler);
		initUartIrq(Nvic_Irq_Flexcom9, "uart9",
			    (rtems_interrupt_handler)&UART9_Handler);
	}
}

static void uartErrorHandler(const Uart_ErrorFlags *errorFlags, void *arg)
{
	(void)arg;
	if (Samrh71RtemsSerial_user_uart_error_callback != NULL) {
		Samrh71RtemsSerial_user_uart_error_callback(
			errorFlags,
			Samrh71RtemsSerial_user_uart_error_callback_arg);
	}
}

typedef struct {
	Pio_Port port;
	Pmc_PeripheralId peripheralId;
	uint32_t pinMask;
	Pio_Control control;
} Samrh71RtemsSerial_UartPinConfig;

static inline Samrh71RtemsSerial_UartPinConfig
Samrh71RtemsSerial_make_uart_pin_config(Pio_Port port,
					Pmc_PeripheralId peripheralId,
					uint32_t pinMask, Pio_Control control)
{
	Samrh71RtemsSerial_UartPinConfig pinConfig = {
		.port = port,
		.peripheralId = peripheralId,
		.pinMask = pinMask,
		.control = control,
	};

	return pinConfig;
}

static inline Uart_Id getUartId(const Serial_SamRH71_Rtems_Device_T device)
{
	switch (device) {
	case Serial_SamRH71_Rtems_Device_T_uart0:
		return Uart_Id_0;
	case Serial_SamRH71_Rtems_Device_T_uart1:
		return Uart_Id_1;
	case Serial_SamRH71_Rtems_Device_T_uart2:
		return Uart_Id_2;
	case Serial_SamRH71_Rtems_Device_T_uart3:
		return Uart_Id_3;
	case Serial_SamRH71_Rtems_Device_T_uart4:
		return Uart_Id_4;
	case Serial_SamRH71_Rtems_Device_T_uart5:
		return Uart_Id_5;
	case Serial_SamRH71_Rtems_Device_T_uart6:
		return Uart_Id_6;
	case Serial_SamRH71_Rtems_Device_T_uart7:
		return Uart_Id_7;
	case Serial_SamRH71_Rtems_Device_T_uart8:
		return Uart_Id_8;
	case Serial_SamRH71_Rtems_Device_T_uart9:
		return Uart_Id_9;
	default:
		assert(false && "Unsupported UART");
		return Uart_Id_0;
	}
}

static inline Flexcom_Id
getFlexcomId(const Serial_SamRH71_Rtems_Device_T device)
{
	switch (device) {
	case Serial_SamRH71_Rtems_Device_T_uart0:
		return Flexcom_Id_0;
	case Serial_SamRH71_Rtems_Device_T_uart1:
		return Flexcom_Id_1;
	case Serial_SamRH71_Rtems_Device_T_uart2:
		return Flexcom_Id_2;
	case Serial_SamRH71_Rtems_Device_T_uart3:
		return Flexcom_Id_3;
	case Serial_SamRH71_Rtems_Device_T_uart4:
		return Flexcom_Id_4;
	case Serial_SamRH71_Rtems_Device_T_uart5:
		return Flexcom_Id_5;
	case Serial_SamRH71_Rtems_Device_T_uart6:
		return Flexcom_Id_6;
	case Serial_SamRH71_Rtems_Device_T_uart7:
		return Flexcom_Id_7;
	case Serial_SamRH71_Rtems_Device_T_uart8:
		return Flexcom_Id_8;
	case Serial_SamRH71_Rtems_Device_T_uart9:
		return Flexcom_Id_9;
	default:
		assert(false && "Unsupported UART");
		return Flexcom_Id_0;
	}
}

static Samrh71RtemsSerial_UartPinConfig
getUartTxPinConfig(const Serial_SamRH71_Rtems_Device_T device)
{
	switch (device) {
	case Serial_SamRH71_Rtems_Device_T_uart0:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_C, Pmc_PeripheralId_Flexcom0, PIO_PIN_21,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart1:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_F, Pmc_PeripheralId_Flexcom1, PIO_PIN_30,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart2:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_A, Pmc_PeripheralId_Flexcom2, PIO_PIN_2,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart3:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_A, Pmc_PeripheralId_Flexcom3, PIO_PIN_20,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart4:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_C, Pmc_PeripheralId_Flexcom4, PIO_PIN_0,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart5:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_C, Pmc_PeripheralId_Flexcom5, PIO_PIN_9,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart6:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_B, Pmc_PeripheralId_Flexcom6, PIO_PIN_6,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart7:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_B, Pmc_PeripheralId_Flexcom7, PIO_PIN_4,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart8:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_A, Pmc_PeripheralId_Flexcom8, PIO_PIN_27,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart9:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_A, Pmc_PeripheralId_Flexcom9, PIO_PIN_25,
			Pio_Control_PeripheralA);

	default:
		assert(false && "Unsupported UART");
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_C, Pmc_PeripheralId_Flexcom0, PIO_PIN_21,
			Pio_Control_PeripheralA);
	}
}

static Samrh71RtemsSerial_UartPinConfig
getUartRxPinConfig(const Serial_SamRH71_Rtems_Device_T device)
{
	switch (device) {
	case Serial_SamRH71_Rtems_Device_T_uart0:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_C, Pmc_PeripheralId_Flexcom0, PIO_PIN_22,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart1:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_F, Pmc_PeripheralId_Flexcom1, PIO_PIN_29,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart2:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_A, Pmc_PeripheralId_Flexcom2, PIO_PIN_6,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart3:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_A, Pmc_PeripheralId_Flexcom3, PIO_PIN_19,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart4:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_C, Pmc_PeripheralId_Flexcom4, PIO_PIN_1,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart5:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_C, Pmc_PeripheralId_Flexcom5, PIO_PIN_10,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart6:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_B, Pmc_PeripheralId_Flexcom6, PIO_PIN_7,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart7:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_B, Pmc_PeripheralId_Flexcom7, PIO_PIN_5,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart8:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_A, Pmc_PeripheralId_Flexcom8, PIO_PIN_28,
			Pio_Control_PeripheralA);

	case Serial_SamRH71_Rtems_Device_T_uart9:
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_A, Pmc_PeripheralId_Flexcom9, PIO_PIN_26,
			Pio_Control_PeripheralA);

	default:
		assert(false && "Unsupported UART");
		return Samrh71RtemsSerial_make_uart_pin_config(
			Pio_Port_C, Pmc_PeripheralId_PioA, PIO_PIN_22,
			Pio_Control_PeripheralA);
	}
}

static inline void
initUartPin(const Samrh71RtemsSerial_UartPinConfig *const pinConfig,
	    Pio_Direction direction)
{
	Pio_Port_Config pioConfig = { .pinsConfig =
				     {
                         .control = pinConfig->control,
                         .direction = direction,
					     .pull = Pio_Pull_Up,
                         .isOpenDrainEnabled = false,
					     .irq = Pio_Irq_EdgeBoth,
                         .isIrqEnabled = false,
                         .driveStrength = Pio_Current_2m,
					     .isSchmittTriggerDisabled = false,
				     },
				     .debounceFilterDiv = 0,
				     .pins = pinConfig->pinMask };
	Pio pio;
	ErrorCode errorCode = 0;

	Pio_init(pinConfig->port, &pio, &errorCode);
	Pio_setPortConfig(&pio, &pioConfig, &errorCode);
}

static inline Pmc_PeripheralId getUartPeripheralId(Uart_Id id)
{
	switch (id) {
	case Uart_Id_0:
		return Pmc_PeripheralId_Flexcom0;
	case Uart_Id_1:
		return Pmc_PeripheralId_Flexcom1;
	case Uart_Id_2:
		return Pmc_PeripheralId_Flexcom2;
	case Uart_Id_3:
		return Pmc_PeripheralId_Flexcom3;
	case Uart_Id_4:
		return Pmc_PeripheralId_Flexcom4;
	case Uart_Id_5:
		return Pmc_PeripheralId_Flexcom5;
	case Uart_Id_6:
		return Pmc_PeripheralId_Flexcom6;
	case Uart_Id_7:
		return Pmc_PeripheralId_Flexcom7;
	case Uart_Id_8:
		return Pmc_PeripheralId_Flexcom8;
	case Uart_Id_9:
		return Pmc_PeripheralId_Flexcom9;
	default:
		assert(false);
		return Pmc_PeripheralId_Flexcom0;
	}
}

static inline void initUartPio(const Serial_SamRH71_Rtems_Device_T device)
{
	const Samrh71RtemsSerial_UartPinConfig txPinConfig =
		getUartTxPinConfig(device);
	const Samrh71RtemsSerial_UartPinConfig rxPinConfig =
		getUartRxPinConfig(device);

	initUartPin(&txPinConfig, Pio_Direction_Output);
	initUartPin(&rxPinConfig, Pio_Direction_Input);
}

inline static void initUartPmc(const Serial_SamRH71_Rtems_Device_T device)
{
	const Samrh71RtemsSerial_UartPinConfig txPinConfig =
		getUartTxPinConfig(device);
	const Samrh71RtemsSerial_UartPinConfig rxPinConfig =
		getUartRxPinConfig(device);

	SamRH71Core_EnablePeripheralClock(txPinConfig.peripheralId);
	/* On most FLEXCOM instances the TX and RX pins share the same peripheral
	 * clock, so a single enable call suffices.  Only enable the RX peripheral
	 * separately when its peripheral ID differs (e.g., split FLEXCOM wiring). */
	if (rxPinConfig.peripheralId != txPinConfig.peripheralId) {
		SamRH71Core_EnablePeripheralClock(rxPinConfig.peripheralId);
	}
	SamRH71Core_EnablePeripheralClock(
		getUartPeripheralId(getUartId(device)));
}

#define FLEXCOM_ADDRESS_BASE 0x40010000U
#define FLEXCOM_ADDRESS_OFFSET 0x00004000U
#define FLEXCOM_MODE_USART 0x01U

inline static void uartInitFlexcom(const Serial_SamRH71_Rtems_Device_T device)
{
	Flexcom_Id id = getFlexcomId(device);
	/* Each FLEXCOM instance occupies 0x4000 bytes of address space starting
	 * at FLEXCOM_ADDRESS_BASE.  The FLEXCOM_MR (mode register) sits at
	 * offset 0 within each instance.  Writing FLEXCOM_MODE_USART (0x01)
	 * selects the USART/UART sub-peripheral; the alternative modes are
	 * SPI (0x02) and TWI/I2C (0x03). */
	uint32_t *flexcomRegister = (uint32_t *)(FLEXCOM_ADDRESS_BASE +
						 (FLEXCOM_ADDRESS_OFFSET * id));

	*flexcomRegister = FLEXCOM_MODE_USART;
}

inline static void uartInitHandle(Uart *uart, Uart_Id id)
{
	switch (id) {
	case Uart_Id_0:
		uart0handle = uart;
		break;
	case Uart_Id_1:
		uart1handle = uart;
		break;
	case Uart_Id_2:
		uart2handle = uart;
		break;
	case Uart_Id_3:
		uart3handle = uart;
		break;
	case Uart_Id_4:
		uart4handle = uart;
		break;
	case Uart_Id_5:
		uart5handle = uart;
		break;
	case Uart_Id_6:
		uart6handle = uart;
		break;
	case Uart_Id_7:
		uart7handle = uart;
		break;
	case Uart_Id_8:
		uart8handle = uart;
		break;
	case Uart_Id_9:
		uart9handle = uart;
		break;
	default:
		assert(false && "Unknown Uart_Id");
	}
}

/** \brief Starts up, initializes and configures Uart and coresponding peripherals
 *
 * \param [in] halUart Hal_Uart structure contains uart device descriptor and relevant fifos.
 * \param [in] halUartConfig configuration structure
 */
static void initUartHardware(Samrh71RtemsSerial_Uart *const halUart,
			     Samrh71RtemsSerial_Uart_Config halUartConfig,
			     const Serial_SamRH71_Rtems_Device_T device)
{
	uartLowLevelInit();

	assert(halUartConfig.id <= Uart_Id_9);
	assert((halUartConfig.parity <= Uart_Parity_Odd) ||
	       (halUartConfig.parity == Uart_Parity_None));

	// init uart
	initUartPmc(device);
	uartInitFlexcom(device);
	initUartPio(device);
	uartInitHandle(&halUart->uart, halUartConfig.id);

	Uart_init(halUartConfig.id, &halUart->uart);
	Uart_reset(&halUart->uart);

	Uart_Config config = { .isTxEnabled = true,
			       .isRxEnabled = true,
			       .isTestModeEnabled = false,
			       .parity = halUartConfig.parity,
			       .stopBits = Uart_StopBits_OneBit,
			       .charLength = Uart_CharacterLength_8Bits,
			       .baudRate = halUartConfig.baudrate,
			       .baudRateClkSrc = Uart_BaudRateClk_PeripheralCk,
			       .baudRateClkFreq =
				       SamRH71Core_GetMainClockFrequency() };
	Uart_setConfig(&halUart->uart, &config);
}

/** \brief Asynchronously sends bytes over uart.
 *
 * \param [in] halUart Hal_Uart structure contains uart device descriptor and relevant fifos.
 * \param [in] buffer array containing bytes to send
 * \param [in] length length of array of bytes
 * \param [in] txHandler pointer to the  handler called after successful array transmission
 */
static void uartWriteAsync(Samrh71RtemsSerial_Uart *const halUart,
			   const uint8_t *const buffer, const uint16_t length,
			   const Uart_TxHandler txHandler)
{
	Uart_ErrorHandler errorHandler = { .callback = uartErrorHandler,
					   .arg = halUart };

	ByteFifo_initFromBytes(&halUart->txFifo, (uint8_t *const)buffer,
			       length);

	Uart_registerErrorHandler(&halUart->uart, errorHandler);
	Uart_writeAsync(&halUart->uart, &halUart->txFifo, txHandler);
}

static void uartWriteBlocking(Samrh71RtemsSerial_Uart *const halUart,
			      const uint8_t *const buffer,
			      const uint16_t length)
{
	ErrorCode errCode = ErrorCode_NoError;
	for (uint16_t i = 0; i < length; i++) {
		Uart_write(&halUart->uart, buffer[i],
			   UART_BLOCKING_WRITE_TIMEOUT, &errCode);
		assert(errCode == ErrorCode_NoError);
	}
}

/** \brief Asynchronously receives bytes over uart.
 *
 * \param [in] halUart Hal_Uart structure contains uart device descriptor and relevant fifos.
 * \param [in] buffer array where received bytes will be stored
 * \param [in] length length of array of bytes
 * \param [in] rxHandler  handler called after successful array reception or after matching character was found
 */
static void uartRead(Samrh71RtemsSerial_Uart *const halUart,
		     uint8_t *const buffer, const uint16_t length,
		     const Uart_RxHandler rxHandler)
{
	Uart_ErrorHandler errorHandler = { .callback = uartErrorHandler,
					   .arg = halUart };
	ByteFifo_init(&halUart->rxFifo, buffer, length);
	Uart_registerErrorHandler(&halUart->uart, errorHandler);
	Uart_readAsync(&halUart->uart, &halUart->rxFifo, rxHandler);
}

static inline void registerUartId(samrh71_rtems_serial_private_data *self,
				  Serial_SamRH71_Rtems_Device_T deviceName)
{
	self->m_hal_uart_config.id = getUartId(deviceName);
}

static inline void initUartParity(samrh71_rtems_serial_private_data *self,
				  Serial_SamRH71_Rtems_Parity_T parity)
{
	switch (parity) {
	case Serial_SamRH71_Rtems_Parity_T_odd:
		self->m_hal_uart_config.parity = Uart_Parity_Odd;
		break;
	case Serial_SamRH71_Rtems_Parity_T_even:
		self->m_hal_uart_config.parity = Uart_Parity_Even;
		break;
	case Serial_SamRH71_Rtems_Parity_T_none:
		self->m_hal_uart_config.parity = Uart_Parity_None;
		break;
	default:
		assert(false && "Not supported parity");
	}
}

static inline void initUartBaudrate(samrh71_rtems_serial_private_data *self,
				    Serial_SamRH71_Rtems_Baudrate_T speed)
{
	switch (speed) {
	case Serial_SamRH71_Rtems_Baudrate_T_b9600:
		self->m_hal_uart_config.baudrate = 9600;
		break;
	case Serial_SamRH71_Rtems_Baudrate_T_b19200:
		self->m_hal_uart_config.baudrate = 19200;
		break;
	case Serial_SamRH71_Rtems_Baudrate_T_b38400:
		self->m_hal_uart_config.baudrate = 38400;
		break;
	case Serial_SamRH71_Rtems_Baudrate_T_b57600:
		self->m_hal_uart_config.baudrate = 57600;
		break;
	case Serial_SamRH71_Rtems_Baudrate_T_b115200:
		self->m_hal_uart_config.baudrate = 115200;
		break;
	case Serial_SamRH71_Rtems_Baudrate_T_b230400:
		self->m_hal_uart_config.baudrate = 230400;
		break;
	default:
		assert(false && "Not supported baudrate");
		break;
	}
}

static inline void
initUart(samrh71_rtems_serial_private_data *const self,
	 const Serial_SamRH71_Rtems_Conf_T *const device_configuration)
{
	self->m_device = device_configuration->devname;
	registerUartId(self, self->m_device);
	initUartParity(self, device_configuration->parity);
	initUartBaudrate(self, device_configuration->speed);
	initUartHardware(&self->m_hal_uart, self->m_hal_uart_config,
			 self->m_device);
}

static void uartRxCallback(void *private_data)
{
	samrh71_rtems_serial_private_data *self =
		(samrh71_rtems_serial_private_data *)private_data;
	rtems_event_send(self->m_task, UART_RX_EVENT);
	/* rtems_status_code releaseResult = */
	/* 	rtems_semaphore_release(self->m_rx_semaphore); */
	/* assert(releaseResult == RTEMS_SUCCESSFUL); */
}

static void initUartRxHandler(samrh71_rtems_serial_private_data *const self)
{
	switch (self->m_packetizer_mode.kind) {
	case raw_PRESENT:
		switch (self->m_packetizer_mode.u.raw.kind) {
		case single_byte_PRESENT:
			self->m_uart_rx_handler.lengthCallback = uartRxCallback;
			self->m_uart_rx_handler.lengthArg = self;
			self->m_uart_rx_handler.characterCallback = NULL;
			self->m_uart_rx_handler.characterArg = NULL;
			self->m_uart_rx_handler.targetLength = 1;
			break;
		case custom_escape_byte_PRESENT:
			self->m_uart_rx_handler.lengthCallback = uartRxCallback;
			self->m_uart_rx_handler.lengthArg = self;
			self->m_uart_rx_handler.characterCallback =
				uartRxCallback;
			self->m_uart_rx_handler.characterArg = self;
			self->m_uart_rx_handler.targetCharacter =
				self->m_packetizer_mode.u.raw.u
					.custom_escape_byte;
			self->m_uart_rx_handler.targetLength =
				Serial_SAMRH71_RTEMS_RECV_BUFFER_SIZE / 2;
			break;
		default:
			// If this branch is hit, then the user provided configuration is invalid,
			// or something went very wrong and the program will enter invalid state, so
			// the safest course of action is to assert and abort (if the asserts are
			// disabled).
			assert(false && "Not supported raw mode kind");
			abort();
		}
		break;
	case escaped_packets_PRESENT:
		self->m_uart_rx_handler.lengthCallback = uartRxCallback;
		self->m_uart_rx_handler.lengthArg = self;
		self->m_uart_rx_handler.characterCallback = uartRxCallback;
		self->m_uart_rx_handler.characterArg = self;
		self->m_uart_rx_handler.targetCharacter = STOP_BYTE;
		self->m_uart_rx_handler.targetLength =
			Serial_SAMRH71_RTEMS_RECV_BUFFER_SIZE / 2;
		break;

		break;
	default:
		// If this branch is hit, then the user provided configuration is invalid,
		// or something went very wrong and the program will enter invalid state, so
		// the safest course of action is to assert and abort (if the asserts are
		// disabled).
		assert(false && "Not supported mode kind");
		abort();
	}
}

static ByteFifo *uartTxCallback(void *private_data)
{
	// called when tx fifo is empty
	samrh71_rtems_serial_private_data *self =
		(samrh71_rtems_serial_private_data *)private_data;

	rtems_status_code releaseResult =
		rtems_semaphore_release(self->m_tx_semaphore);
	assert(releaseResult == RTEMS_SUCCESSFUL);
	return NULL;
}

static void initUartTxHandler(samrh71_rtems_serial_private_data *const self)
{
	self->m_uart_tx_handler.callback = uartTxCallback;
	self->m_uart_tx_handler.arg = self;

	const rtems_status_code status_code =
		rtems_semaphore_create(SamRH71Core_GenerateNewSemaphoreName(),
				       1, // Initial value, unlocked
				       RTEMS_SIMPLE_BINARY_SEMAPHORE,
				       0, // Priority ceiling
				       &self->m_tx_semaphore);

	assert(status_code == RTEMS_SUCCESSFUL);
}

static inline bool
rawModeEnabled(const samrh71_rtems_serial_private_data *const self)
{
	return self->m_packetizer_mode.kind == raw_PRESENT;
}

void Samrh71RtemsSerialInit(
	void *private_data, const enum SystemBus bus_id,
	const enum SystemDevice device_id,
	const Serial_SamRH71_Rtems_Conf_T *const device_configuration,
	const Serial_SamRH71_Rtems_Conf_T *const remote_device_configuration)
{
	(void)device_id;
	(void)remote_device_configuration;

	samrh71_rtems_serial_private_data *self =
		(samrh71_rtems_serial_private_data *)private_data;

	self->m_ip_device_bus_id = bus_id;
	self->m_packetizer_mode = device_configuration->packetizer_mode;

	initUart(self, device_configuration);
	initUartRxHandler(self);
	initUartTxHandler(self);

	if (!rawModeEnabled(self)) {
		Escaper_init(&self->m_escaper, self->m_encoded_packet_buffer,
			     Serial_SAMRH71_RTEMS_ENCODED_PACKET_MAX_SIZE,
			     self->m_decoded_packet_buffer,
			     Serial_SAMRH71_RTEMS_DECODED_PACKET_MAX_SIZE);
	}

	rtems_task_config taskConfig = {
		.name = SamRH71Core_GenerateNewTaskName(),
		.initial_priority = 1,
		.storage_area = self->m_task_buffer,
		.storage_size = Serial_SAMRH71_RTEMS_TASK_BUFFER_SIZE,
		.maximum_thread_local_storage_size =
			Serial_SAMRH71_RTEMS_UART_TLS_SIZE,
		.storage_free = NULL,
		.initial_modes = RTEMS_PREEMPT,
		.attributes = RTEMS_DEFAULT_ATTRIBUTES | RTEMS_FLOATING_POINT
	};

	const rtems_status_code taskConstructionResult =
		rtems_task_construct(&taskConfig, &self->m_task);
	assert(taskConstructionResult == RTEMS_SUCCESSFUL);

	const rtems_status_code taskStartStatus = rtems_task_start(
		self->m_task, (rtems_task_entry)&Samrh71RtemsSerialPoll,
		(rtems_task_argument)self);
	assert(taskStartStatus == RTEMS_SUCCESSFUL);
}

void Samrh71RtemsSerialPoll(rtems_task_argument private_data)
{
	samrh71_rtems_serial_private_data *self =
		(samrh71_rtems_serial_private_data *)private_data;

	if (!rawModeEnabled(self)) {
		// if raw mode is disabled, start the Escaper's decoder
		Escaper_start_decoder(&self->m_escaper);
	}
	/* /\* Prime the first asynchronous read before entering the poll loop. */
	/*  * The RX semaphore is initially unlocked (value=1) so this obtain */
	/*  * returns immediately and arms the hardware FIFO read.  Subsequent */
	/*  * iterations block in the loop below until bytes arrive. *\/ */
	/* rtems_status_code obtainResult = rtems_semaphore_obtain( */
	/* 	self->m_rx_semaphore, RTEMS_WAIT, RTEMS_NO_TIMEOUT); */
	/* assert(obtainResult == RTEMS_SUCCESSFUL); */
	/* uartRead(&self->m_hal_uart, */
	/* 			     self->m_fifo_memory_block, */
	/* 			     Serial_SAMRH71_RTEMS_RECV_BUFFER_SIZE, */
	/* 			     self->m_uart_rx_handler); */

	uartRead(&self->m_hal_uart, self->m_fifo_memory_block,
		 Serial_SAMRH71_RTEMS_RECV_BUFFER_SIZE,
		 self->m_uart_rx_handler);

	while (true) {
		rtems_event_set received_events = 0;
		/// Wait for data to arrive - event shall be triggered by ISR
		const rtems_status_code eventStatus = rtems_event_receive(
			UART_RX_EVENT, RTEMS_WAIT | RTEMS_EVENT_ANY,
			RTEMS_NO_TIMEOUT, &received_events);

		if (eventStatus == RTEMS_SUCCESSFUL &&
		    (received_events & UART_RX_EVENT)) {
			ByteFifo byteFifo;
			ByteFifo_init(&byteFifo, self->m_recv_buffer,
				      Serial_SAMRH71_RTEMS_RECV_BUFFER_SIZE);
			Uart_readRxFifo(&self->m_hal_uart.uart, &byteFifo);
			const size_t length = ByteFifo_getCount(&byteFifo);
			if (rawModeEnabled(self)) {
				for (size_t i = 0; i < length; i++) {
					// if raw mode is enabled, call the Broker directly
					Broker_receive_packet(
						self->m_ip_device_bus_id,
						&self->m_recv_buffer[i], 1);
				}
			} else {
				// if raw mode is disabled, use Escaper
				Escaper_decode_packet(&self->m_escaper,
						      self->m_ip_device_bus_id,
						      self->m_recv_buffer,
						      length,
						      Broker_receive_packet);
			}
		}

		// Wait for data to arrive. Semaphore will be given
		/* obtainResult = rtems_semaphore_obtain( */
		/* 	self->m_rx_semaphore, RTEMS_WAIT, RTEMS_NO_TIMEOUT); */
		/* assert(obtainResult == RTEMS_SUCCESSFUL); */

		/* ByteFifo byteFifo; */
		/* ByteFifo_init(&byteFifo, self->m_recv_buffer, */
		/* 	      Serial_SAMRH71_RTEMS_RECV_BUFFER_SIZE); */
		/* Uart_readRxFifo(&self->m_hal_uart.uart, &byteFifo); */
		/* const size_t length = ByteFifo_getCount(&byteFifo); */
	}
}

/**
 * @brief Write callback for blocking TX mode: direct register write.
 */
static void blockingWritePacket(samrh71_rtems_serial_private_data *const self,
				const uint8_t *const buffer,
				const uint16_t length)
{
	uartWriteBlocking(&self->m_hal_uart, buffer, length);
}

/**
 * @brief Packet write function signature.
 *
 * Used to abstract over blocking vs async TX modes.
 * Receives @p self to access both the UART and the TX semaphore.
 */
typedef void (*SamRH71RtemsSerial_WritePacketFn)(
	samrh71_rtems_serial_private_data *const self,
	const uint8_t *const buffer, const uint16_t length);

/**
 * @brief Encode data with Escaper and send each encoded packet.
 *
 * Calls writePacket for every encoded packet after running the
 * Escaper encoder over the full input.
 */
static void sendEscapedPackets(samrh71_rtems_serial_private_data *const self,
			       const uint8_t *const data, const size_t length,
			       SamRH71RtemsSerial_WritePacketFn writePacket)
{
	Escaper_start_encoder(&self->m_escaper);

	size_t index = 0;
	while (index < length) {
		const size_t packetLength = Escaper_encode_packet(
			&self->m_escaper, data, length, &index);

		writePacket(self, self->m_encoded_packet_buffer, packetLength);
	}
}

void Samrh71RtemsSerialSend(void *private_data, const uint8_t *const data,
			    const size_t length)
{
	samrh71_rtems_serial_private_data *self =
		(samrh71_rtems_serial_private_data *)private_data;
	size_t index = 0;
	size_t packetLength = 0;

	if (!rawModeEnabled(self)) {
		sendEscapedPackets(self, data, length, blockingWritePacket);
	} else {
		uartWriteBlocking(&self->m_hal_uart, data, length);
	}
}

void Samrh71RtemsSerialRegisterUserUartErrorCallback(
	Samrh71RtemsSerial_UserUartErrorCallback callback, void *arg)
{
	Samrh71RtemsSerial_user_uart_error_callback = callback;
	Samrh71RtemsSerial_user_uart_error_callback_arg = arg;
}
