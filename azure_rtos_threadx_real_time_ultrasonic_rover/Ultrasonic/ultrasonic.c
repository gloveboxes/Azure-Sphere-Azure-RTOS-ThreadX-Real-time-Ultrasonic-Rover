#include "ultransonic.h"

void write_reg32(uintptr_t baseAddr, size_t offset, uint32_t value)
{
	*(volatile uint32_t*)(baseAddr + offset) = value;
}

uint32_t read_reg32(uintptr_t baseAddr, size_t offset)
{
	return *(volatile uint32_t*)(baseAddr + offset);
}

void gpt3_wait_microseconds(int microseconds)
{
	// GPT3_INIT = initial counter value
	write_reg32(GPT_BASE, 0x54, 0x0);

	// GPT3_CTRL
	uint32_t ctrlOn = 0x0;
	ctrlOn |= (0x19) << 16; // OSC_CNT_1US (default value)
	ctrlOn |= 0x1;          // GPT3_EN = 1 -> GPT3 enabled
	write_reg32(GPT_BASE, 0x50, ctrlOn);

	// GPT3_CNT
	while (read_reg32(GPT_BASE, 0x58) < microseconds)
	{
		// empty.
	}

	// GPT_CTRL -> disable timer
	write_reg32(GPT_BASE, 0x50, 0x0);
}

bool read_input(u8 pin)
{
	os_hal_gpio_data value = 0;
	mtk_os_hal_gpio_get_input(pin, &value);
	return value == OS_HAL_GPIO_DATA_HIGH;
}

float lp_get_distance(u8 pin, unsigned long timeoutMicroseconds)
{
	uint32_t pulseBegin, pulseEnd;

	mtk_os_hal_gpio_set_direction(pin, OS_HAL_GPIO_DIR_OUTPUT);	// set for output
	mtk_os_hal_gpio_set_output(pin, OS_HAL_GPIO_DATA_LOW);		// pull low
	gpt3_wait_microseconds(2);

	mtk_os_hal_gpio_set_output(pin, OS_HAL_GPIO_DATA_HIGH);		// pull high
	gpt3_wait_microseconds(5);

	// GPT3_CTRL - starts microsecond resolution clock
	uint32_t ctrlOn = 0x0;
	ctrlOn |= (0x19) << 16; // OSC_CNT_1US (default value)
	ctrlOn |= 0x1;          // GPT3_EN = 1 -> GPT3 enabled
	write_reg32(GPT_BASE, 0x50, ctrlOn);

	mtk_os_hal_gpio_set_direction(pin, OS_HAL_GPIO_DIR_INPUT);	// set for input

	while (read_input(pin))		// wait for any previous pulse to end
	{
		if (read_reg32(GPT_BASE, 0x58) > timeoutMicroseconds)
		{
			write_reg32(GPT_BASE, 0x50, 0x0);	// GPT_CTRL -> disable timer
			return NAN;
		}
	}

	while (!read_input(pin))		// wait for the pulse to start
	{
		pulseBegin = read_reg32(GPT_BASE, 0x58);
		if (read_reg32(GPT_BASE, 0x58) > timeoutMicroseconds)
		{
			write_reg32(GPT_BASE, 0x50, 0x0);	// GPT_CTRL -> disable timer
			return NAN;
		}
	}

	pulseBegin = read_reg32(GPT_BASE, 0x58);

	while (read_input(pin))		// wait for the pulse to stop
	{
		if (read_reg32(GPT_BASE, 0x58) > timeoutMicroseconds)
		{
			write_reg32(GPT_BASE, 0x50, 0x0);	// GPT_CTRL -> disable timer
			return NAN;
		}
	}

	pulseEnd = read_reg32(GPT_BASE, 0x58);

	write_reg32(GPT_BASE, 0x50, 0x0);	// GPT_CTRL -> disable timer

	return (pulseEnd - pulseBegin) / 58.0; //  (29 / 2);
}
