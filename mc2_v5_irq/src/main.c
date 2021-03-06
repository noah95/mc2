/*
 * m2_v1_FreeRTOS_Intro_Vorlage
 * (c) Matthias Meier 2017
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "semphr.h"
#include "task.h"


#include "Timer.h"
#include "BlinkLed.h"
#include "Lcd.h"
#include "stm32f429i_discovery_lcd.h"

#define PINK (0xff<<24) | (0xd6<<16) | (0x64<<8) | (0xbe<<0)
#define BLUE (0xff<<24) | (0x00<<16) | (0x00<<8) | (0xff<<0)
#define RED (0xff<<24) | (0xff<<16) | (0x00<<8) | (0x00<<0)
#define BLACK (0xff<<24) | (0x00<<16) | (0x00<<8) | (0x00<<0)

#define RGB_TO_ARGB(x) ( (0xff<<24) | ( x ) )

/* mm: added helper variable to support FreeRTOS thread list support in gdb
 * Remark: OpenOCD gdbserver references xTopReadyPriority which was removed in newer FreeRTOS releases.
 * The declaration below together with linker flag '-Wl,--undefined=uxTopUsedPriority' solves the problem */
const int __attribute__((used)) uxTopUsedPriority = configMAX_PRIORITIES;

SemaphoreHandle_t LCDSemaphore;
extern SemaphoreHandle_t ISTSemaphore;
uint32_t tim6_at_isr;
uint32_t tim6_at_ist;

static void CounterTask(void *pvParameters);
static void HeartbeatTask(void *pvParameters);
static void WorkerTask(void *pvParameters);
static void TimerServiceTask(void *pvParameters);
static void lcd_printf(uint32_t Color, uint16_t X, uint16_t Y, Text_AlignModeTypdef mode, const char *fmt, ...) __attribute__((__format__(__printf__,5,6)));


int main(void)
{
	SysTick->CTRL = 0; // Disable SYSTICK timer to init LCD
	blink_led_init();
	lcd_init();
	BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);

//	LCDSemaphore = xSemaphoreCreateBinary();
//	xSemaphoreGive( LCDSemaphore );
	LCDSemaphore = xSemaphoreCreateMutex();

	xTaskCreate(CounterTask, "CntTask", (configMINIMAL_STACK_SIZE + 50), NULL, (tskIDLE_PRIORITY + 3), NULL);
	xTaskCreate(HeartbeatTask, "HeartTask", (configMINIMAL_STACK_SIZE + 50), NULL, (tskIDLE_PRIORITY + 2), NULL);
	xTaskCreate(WorkerTask, "WorkTask", (configMINIMAL_STACK_SIZE + 50), NULL, (tskIDLE_PRIORITY + 1), NULL);

	xTaskCreate(TimerServiceTask, "TimerServiceTask", (configMINIMAL_STACK_SIZE + 50), NULL, (tskIDLE_PRIORITY + 4), NULL);


	ISTSemaphore = xSemaphoreCreateBinary();
	vTaskStartScheduler();

	/* the FreeRTOS scheduler never returns to here except when out of memory at creating the idle task */
	for (;;) ;
}

static void lcd_printf(uint32_t Color, uint16_t X, uint16_t line, Text_AlignModeTypdef mode, const char *fmt, ...)
{
	char s[30];

    va_list ap;
	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);

	if( xSemaphoreTake( LCDSemaphore, ( TickType_t ) 10 ) == pdTRUE )
	{
		BSP_LCD_SetTextColor(Color);
		BSP_LCD_DisplayStringAt(X, LINE(line), s, mode);
		xSemaphoreGive( LCDSemaphore );
	}
}

static void CounterTask(__attribute__ ((unused)) void *pvParameters)
{
	int n = 0;
	int take_err = 0;
	UBaseType_t uxHighWaterMark = 0;
	BSP_LCD_DisplayStringAtLine(0, "mc2 Vorlage");

	while (1) {
		char s[20];

		blink_led_on();
		vTaskDelay(1);
		blink_led_off();
		vTaskDelay(1);
		uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
		snprintf(s, sizeof(s), "C=%d S=%d E=%d", n++, (unsigned int)uxHighWaterMark, take_err);
//		snprintf(s, sizeof(s), "C=%d", n++);


		lcd_printf(RED, 0, 4, CENTER_MODE, "C=%d S=%d E=%d", n++, (unsigned int)uxHighWaterMark, take_err);

	}
}


static void TimerServiceTask(__attribute__ ((unused)) void *pvParameters)
{
	static unsigned int ctr = 0;
	while(1)
	{
		if( xSemaphoreTake( ISTSemaphore, ( TickType_t ) 10 ) == pdTRUE )
		{
			tim6_at_ist = TIM6->CNT;
			lcd_printf(PINK, 0, 6, LEFT_MODE, "IST COUNT %d", ctr++);
			lcd_printf(PINK, 0, 7, LEFT_MODE, "tim6_at_isr %d (%0.1fus)", tim6_at_isr, tim6_at_isr/160.0f);
			lcd_printf(PINK, 0, 8, LEFT_MODE, "tim6_at_ist %d", tim6_at_ist);
			lcd_printf(RGB_TO_ARGB(0xA2D729), 0, 9, LEFT_MODE, "tim6 delta %d", tim6_at_ist-tim6_at_isr);
		}
	}
}
static void HeartbeatTask(__attribute__ ((unused)) void *pvParameters)
{
	while (1) {
		blink_led2_on();
		vTaskDelay(100);
		while(BSP_PB_GetState(BUTTON_KEY) != 0);
		blink_led2_off();
		vTaskDelay(100);
		while(BSP_PB_GetState(BUTTON_KEY) != 0);
		blink_led2_on();
		vTaskDelay(100);
		while(BSP_PB_GetState(BUTTON_KEY) != 0);
		blink_led2_off();
		vTaskDelay(700);
		while(BSP_PB_GetState(BUTTON_KEY) != 0);
	}
}

static void WorkerTask(__attribute__ ((unused)) void *pvParameters)
{
//	lcd_init();
//	while(1);
	vTaskDelay(50);
	while (1) {
		/* See if we can obtain the semaphore.  If the semaphore is not
			available wait 10 ticks to see if it becomes free. */
		if( xSemaphoreTake( LCDSemaphore, ( TickType_t ) 10 ) == pdTRUE )
		{
			/* We were able to obtain the semaphore and can now access the
				shared resource. */
//			BSP_LCD_SetTextColor(BLUE);
//			BSP_LCD_DisplayStringAtLine(1, "Hi from Worker Task!");
			lcd_printf(BLUE, 0, 1, LEFT_MODE, "Hi from Worker Task!");
			/* We have finished accessing the shared resource.  Release the
				semaphore. */
			xSemaphoreGive( LCDSemaphore );
		}
		else
		{
			/* We could not obtain the semaphore and can therefore not access
				the shared resource safely. */
		}

	}
}

// ----------------------------------------------------------------------------
