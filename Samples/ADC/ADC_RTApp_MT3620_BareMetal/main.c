/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#include "mt3620-baremetal.h"
#include "mt3620-timer-poll.h"
#include "mt3620-uart-poll.h"
#include "mt3620-adc.h"

#define LOCAL_C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]

extern uint32_t StackTop; // &StackTop == end of TCM0

static _Noreturn void DefaultExceptionHandler(void);

static _Noreturn void RTCoreMain(void);
// The exception vector table contains a stack pointer, 15 exception handlers, and an entry for
// each interrupt.
#define INTERRUPT_COUNT 100 // from datasheet
#define EXCEPTION_COUNT (16 + INTERRUPT_COUNT)
#define INT_TO_EXC(i_) (16 + (i_))
#define EXCEPTION_TABLE_EXPECTED_SIZE ( EXCEPTION_COUNT * sizeof(uintptr_t) )

// ARM DDI0403E.d SB1.5.2-3
// From SB1.5.3, "The Vector table must be naturally aligned to a power of two whose alignment
// value is greater than or equal to (Number of Exceptions supported x 4), with a minimum alignment
// of 128 bytes.". The array is aligned in linker.ld, using the dedicated section ".vector_table".
#define EXCEPTION_TABLE_ALIGNMENT (POW2_CEIL(EXCEPTION_TABLE_EXPECTED_SIZE) < 128 ? 128 : POW2_CEIL(EXCEPTION_TABLE_EXPECTED_SIZE))
typedef struct { uintptr_t v[EXCEPTION_COUNT]; } __attribute__((aligned(EXCEPTION_TABLE_ALIGNMENT))) EXCEPTION_VECTOR_TABLE;

LOCAL_C_ASSERT(sizeof(EXCEPTION_VECTOR_TABLE) == EXCEPTION_TABLE_ALIGNMENT);

static const EXCEPTION_VECTOR_TABLE ExceptionVectorTable
	__attribute__((section(".vector_table")))
	__attribute__((used))
	= {
		.v[0] = (uintptr_t)& StackTop, // Main Stack Pointer (MSP)
		.v[1] = (uintptr_t)RTCoreMain,               // Reset
		.v[2] = (uintptr_t)DefaultExceptionHandler,  // NMI
		.v[3] = (uintptr_t)DefaultExceptionHandler,  // HardFault
		.v[4] = (uintptr_t)DefaultExceptionHandler,  // MPU Fault
		.v[5] = (uintptr_t)DefaultExceptionHandler,  // Bus Fault
		.v[6] = (uintptr_t)DefaultExceptionHandler,  // Usage Fault
		.v[11] = (uintptr_t)DefaultExceptionHandler, // SVCall
		.v[12] = (uintptr_t)DefaultExceptionHandler, // Debug monitor
		.v[14] = (uintptr_t)DefaultExceptionHandler, // PendSV
		.v[15] = (uintptr_t)DefaultExceptionHandler, // SysTick
		.v[INT_TO_EXC(0)... INT_TO_EXC(INTERRUPT_COUNT - 1)] = (uintptr_t)DefaultExceptionHandler
	};

static _Noreturn void DefaultExceptionHandler(void)
{
	for (;;) {
		// empty.
	}
}

static _Noreturn void RTCoreMain(void)
{
	// SCB->VTOR = ExceptionVectorTable
	WriteReg32(SCB_BASE, 0x08, (uint32_t)&ExceptionVectorTable);

    Uart_Init();
    Uart_WriteStringPoll("--------------------------------\r\n");
    Uart_WriteStringPoll("ADC_RTApp_MT3620_BareMetal\r\n");
    Uart_WriteStringPoll("App built on: " __DATE__ ", " __TIME__ "\r\n");

    EnableAdc();

    // Print voltage on channel zero every second.
    for (;;) {
        Gpt3_WaitUs(1000 * 1000);

        uint32_t value = ReadAdc(0);

        // Write whole-part, ".", fractional-part
        uint32_t mV = (value * 2500) / 0xFFF;
        Uart_WriteIntegerPoll(mV / 1000);
        Uart_WriteStringPoll(".");
        Uart_WriteIntegerWidthPoll(mV % 1000, 3);
        Uart_WriteStringPoll("\r\n");
    }
}
