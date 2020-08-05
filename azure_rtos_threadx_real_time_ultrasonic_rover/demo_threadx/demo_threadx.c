/* This is a small demo of the high-performance ThreadX kernel.  It includes examples of eight
   threads of different priorities, using a message queue, semaphore, mutex, event flags group,
   byte pool, and block pool.  */

#include "tx_api.h"
#include "printf.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include "os_hal_gpio.h"
#include "Ultrasonic/ultransonic.h"

#include "hw/azure_sphere_learning_path.h"

bool HLAppReady = false;
float distance_left, distance_right, newDistanceLeft, newDistanceRight;
bool newDataReady = false;

enum LEDS
{
	RED,
	GREEN,
	BLUE
};

typedef struct ULTRASONIC_SENSOR
{
	float centimeters;
	enum LEDS previous_led;
	int LEDs[];
} ULTRASONIC_SENSOR;

ULTRASONIC_SENSOR leftSensor = { .LEDs = { LED_RED, LED_GREEN, LED_BLUE } };
ULTRASONIC_SENSOR rightSensor = { .LEDs = { LED_RED_RIGHT, LED_GREEN_RIGHT, LED_BLUE_RIGHT } };


#define DEMO_STACK_SIZE         1024
#define DEMO_BYTE_POOL_SIZE     9120
#define DEMO_BLOCK_POOL_SIZE    100
#define DEMO_QUEUE_SIZE         100


/* Define the ThreadX object control blocks...  */

TX_THREAD               thread_measure_distance;
TX_BYTE_POOL            byte_pool_0;

UCHAR                   memory_area[DEMO_BYTE_POOL_SIZE];


/* Define thread prototypes.  */
void thread_measure_distance_entry(ULONG thread_input);


int main(void)
{

	/* Enter the ThreadX kernel.  */
	tx_kernel_enter();
}


/* Define what the initial system looks like.  */

void tx_application_define(void* first_unused_memory)
{
	CHAR* pointer;

	/* Create a byte memory pool from which to allocate the thread stacks.  */
	tx_byte_pool_create(&byte_pool_0, "byte pool 0", memory_area, DEMO_BYTE_POOL_SIZE);

	/* Put system definition stuff in here, e.g. thread creates and other assorted
	   create information.  */

	   /* Allocate the stack for thread 1.  */
	tx_byte_allocate(&byte_pool_0, (VOID**)&pointer, DEMO_STACK_SIZE, TX_NO_WAIT);

	tx_thread_create(&thread_measure_distance, "thread measure", thread_measure_distance_entry, 0,
		pointer, DEMO_STACK_SIZE, 1, 1, TX_NO_TIME_SLICE, TX_AUTO_START);
}


int gpio_output(u8 gpio_no, u8 level)
{
	int ret;

	ret = mtk_os_hal_gpio_request(gpio_no);
	if (ret != 0)
	{
		printf("request gpio[%d] fail\n", gpio_no);
		return ret;
	}
	mtk_os_hal_gpio_set_direction(gpio_no, OS_HAL_GPIO_DIR_OUTPUT);
	mtk_os_hal_gpio_set_output(gpio_no, level);
	ret = mtk_os_hal_gpio_free(gpio_no);
	if (ret != 0)
	{
		printf("free gpio[%d] fail\n", gpio_no);
		return ret;
	}
	return 0;
}


// https://embeddedartistry.com/blog/2017/02/17/implementing-malloc-with-threadx/
// overrides for malloc and free required for srand and rand
void *malloc(size_t size)
{
	void* ptr = NULL;

	if (size > 0)
	{
		// We simply wrap the threadX call into a standard form
		uint8_t r = tx_byte_allocate(&byte_pool_0, &ptr, size,
			TX_WAIT_FOREVER);

		if (r != TX_SUCCESS)
		{
			ptr = NULL;
		}
	}
	//else NULL if there was no size

	return ptr;
}


void free(void* ptr)
{
	if (ptr)
	{
		//We simply wrap the threadX call into a standard form
		//uint8_t r = tx_byte_release(ptr);
		tx_byte_release(ptr);
	}
}


void set_distance_indicator(ULTRASONIC_SENSOR* sensor)
{
	enum LEDS current_led = RED;

	if (!isnan(sensor->centimeters))
	{
		current_led = sensor->centimeters > 20.0 ? GREEN : sensor->centimeters > 10.0 ? BLUE : RED;
		if (sensor->previous_led != current_led)
		{
			gpio_output(sensor->LEDs[(int)sensor->previous_led], true); // turn off old current colour
			sensor->previous_led = current_led;
		}

		gpio_output(sensor->LEDs[(int)current_led], false);
	}
	else
	{
		for (size_t i = 0; i < 3; i++)
		{
			gpio_output(sensor->LEDs[i], true);
		}
	}
}


void thread_measure_distance_entry(ULONG thread_input)
{
	while (true)
	{
		leftSensor.centimeters = lp_get_distance(HCSR04_LEFT, 3000);
		set_distance_indicator(&leftSensor);

		rightSensor.centimeters = lp_get_distance(HCSR04_RIGHT, 3000);
		set_distance_indicator(&rightSensor);

		tx_thread_sleep(1);
	}
}
