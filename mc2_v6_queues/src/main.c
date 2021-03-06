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

#define COMMAND_UP 1
#define COMMAND_DOWN 2
typedef struct
 {
    int command;
 } xMessage_t;
QueueHandle_t xQueue;

static void ButtonTask(void *pvParameters);
static void ElevatorTask(void *pvParameters);
static void lcd_printf(uint32_t Color, uint16_t X, uint16_t Y, Text_AlignModeTypdef mode, const char *fmt, ...) __attribute__((__format__(__printf__,5,6)));


int main(void)
{
	SysTick->CTRL = 0; // Disable SYSTICK timer to init LCD
	blink_led_init();
	lcd_init();
	BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);

	LCDSemaphore = xSemaphoreCreateMutex();

	xQueue = xQueueCreate( 10, sizeof( xMessage_t ) );

	xTaskCreate(ButtonTask, "ButtonTask", (configMINIMAL_STACK_SIZE + 50), NULL, (tskIDLE_PRIORITY + 3), NULL);
	xTaskCreate(ElevatorTask, "ElevatorTask", (configMINIMAL_STACK_SIZE + 50), NULL, (tskIDLE_PRIORITY + 2), NULL);

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

static void ButtonTask(__attribute__ ((unused)) void *pvParameters)
{
	xMessage_t msg;

	lcd_printf(BLACK, 0, 0, CENTER_MODE, "mc2_v6_queues");

	while (1)
	{
		while(BSP_PB_GetState(BUTTON_KEY) == 0) vTaskDelay(10);
		if(rand() & 0x1) msg.command = COMMAND_UP;
		else msg.command = COMMAND_DOWN;
		xQueueSend(xQueue, &msg, 0);
		vTaskDelay(100);
		while(BSP_PB_GetState(BUTTON_KEY) != 0);
	}
}

static void ElevatorTask(__attribute__ ((unused)) void *pvParameters)
{
	xMessage_t msg;
	int ctr = 2;
	BaseType_t ret = pdFALSE;

	while(1)
	{
		ret = pdFALSE;
		while (ret == pdFALSE) ret = xQueueReceive(xQueue, &msg, 10);
		switch(msg.command)
		{
			case COMMAND_UP:
				lcd_printf(BLACK, 0, ctr, LEFT_MODE, "COMMAND_UP");
				break;
			case COMMAND_DOWN:
				lcd_printf(BLACK, 0, ctr, LEFT_MODE, "COMMAND_DOWN");
				break;
		}
		ctr++;
		vTaskDelay(1000);
		if(ctr == 17) ctr = 2;
	}
}

// ----------------------------------------------------------------------------
