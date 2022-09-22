#include "conf_board.h"
#include <asf.h>
#include "defines.h"
#include "helpers.h"

/************************************************************************/
/* BOARD CONFIG                                                         */
/************************************************************************/

#define BUT_PIO PIOA
#define BUT_PIO_ID ID_PIOA
#define BUT_PIO_PIN 11
#define BUT_IDX_MASK (1 << BUT_PIO_PIN)

#define USART_COM_ID ID_USART1
#define USART_COM USART1

/************************************************************************/
/* RTOS                                                                */
/************************************************************************/

#define TASK_LED_STACK_SIZE (4096*2 / sizeof(portSTACK_TYPE))
#define TASK_LED_STACK_PRIORITY (tskIDLE_PRIORITY)
#define TASK_BUT_STACK_SIZE (4096*2 / sizeof(portSTACK_TYPE))
#define TASK_BUT_STACK_PRIORITY (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
                                          signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

/************************************************************************/
/* recursos RTOS                                                        */
/************************************************************************/

/** Semaforo a ser usado pela task led */
SemaphoreHandle_t xSemaphoreBut;
SemaphoreHandle_t xSemaphoreBut1;

/** Queue for msg log send data */
QueueHandle_t xQueueLedFreq;
QueueHandle_t xQueueButPress;

/************************************************************************/
/* prototypes local                                                     */
/************************************************************************/

void but_callback(void);
static void BUT_init(void);
void pin_toggle(Pio *pio, uint32_t mask);
static void USART1_init(void);
void LED_init(int estado);

/************************************************************************/
/* RTOS application funcs                                               */
/************************************************************************/

/**
 * \brief Called if stack overflow during execution
 */
extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
                                          signed char *pcTaskName) {
  printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
  /* If the parameters have been corrupted then inspect pxCurrentTCB to
   * identify which task has overflowed its stack.
   */
  for (;;) {
  }
}

/**
 * \brief This function is called by FreeRTOS idle task
 */
extern void vApplicationIdleHook(void) { pmc_sleep(SAM_PM_SMODE_SLEEP_WFI); }

/**
 * \brief This function is called by FreeRTOS each tick
 */
extern void vApplicationTickHook(void) {}

extern void vApplicationMallocFailedHook(void) {
  /* Called if a call to pvPortMalloc() fails because there is insufficient
  free memory available in the FreeRTOS heap.  pvPortMalloc() is called
  internally by FreeRTOS API functions that create tasks, queues, software
  timers, and semaphores.  The size of the FreeRTOS heap is set by the
  configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

  /* Force an assert. */
  configASSERT((volatile void *)NULL);
}

/************************************************************************/
/* handlers / callbacks                                                 */
/************************************************************************/

void but_callback(void) {
	// Semaphore
	  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	//   xSemaphoreGiveFromISR(xSemaphoreBut, &xHigherPriorityTaskWoken);
	
	//Queue
	int value = -100;
	xQueueSendFromISR(xQueueButPress, &value, &xHigherPriorityTaskWoken);
}

void but1_callback(void) {
	// Semaphore
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	// 	xSemaphoreGiveFromISR(xSemaphoreBut1, &xHigherPriorityTaskWoken);
	
	//Queue
	int value = 100;
	xQueueSendFromISR(xQueueButPress,  &value, &xHigherPriorityTaskWoken);
}
/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_led(void *pvParameters) {

  LED_init(1);

  uint32_t msg = 0;
  uint32_t delayMs = 500;

  for (;;) {
    if (xQueueReceive(xQueueLedFreq, &msg, (TickType_t)0)) {
      delayMs = msg / portTICK_PERIOD_MS;
      printf("delay = %d \n", delayMs);
    }

    pin_toggle(LED_PIO, LED_IDX_MASK);

    vTaskDelay(delayMs);
  }
}

static void task_but_semaphore(void *pvParameters) {
	char changed = 0;
  BUT_init();

  uint32_t delayTicks = 2000;

  for (;;) {
    if (xSemaphoreTake(xSemaphoreBut, 0)) {
      delayTicks -= 100;
			changed = 1;
		}
		if(xSemaphoreTake(xSemaphoreBut1, 0)) {
			 delayTicks += 100;
			 changed = 1;
		}
		if(changed){
			changed = 0;
			xQueueSend(xQueueLedFreq, (void *)&delayTicks, 10);

			if (delayTicks == 100) {
				delayTicks = 900;
			}
		}
  }
}

static void task_but(void *pvParameters) {
	BUT_init();
	int msg = 0;
	uint32_t delayTicks = 2000;
	for (;;) {
		if (xQueueReceive(xQueueButPress, &msg, (TickType_t)0)) {
			delayTicks += msg;
			xQueueSend(xQueueLedFreq, (void *)&delayTicks, 10);
    }
	}
}

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

/**
 * \brief Configure the console UART.
 */
static void configure_console(void) {
  const usart_serial_options_t uart_serial_options = {
      .baudrate = CONF_UART_BAUDRATE,
      .charlength = CONF_UART_CHAR_LENGTH,
      .paritytype = CONF_UART_PARITY,
      .stopbits = CONF_UART_STOP_BITS,
  };

  stdio_serial_init(CONF_UART, &uart_serial_options);

  setbuf(stdout, NULL);
}

void pin_toggle(Pio *pio, uint32_t mask) {
  if (pio_get_output_data_status(pio, mask))
    pio_clear(pio, mask);
  else
    pio_set(pio, mask);
}

void LED_init(int estado){
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_set_output(LED_PIO, LED_IDX_MASK, estado, 0, 0);
};


static void BUT_init(void) {
	config_button(BUT_PIO, BUT_IDX_MASK, BUT_PIO_ID, but_callback, 1, 1);
	config_button(BUT1_PIO, BUT1_IDX_MASK, BUT1_PIO_ID, but1_callback, 1, 1);

}

/************************************************************************/
/* main                                                                 */
/************************************************************************/

/**
 *  \brief FreeRTOS Real Time Kernel example entry point.
 *
 *  \return Unused (ANSI-C compatibility).
 */
int main(void) {
  /* Initialize the SAM system */
  sysclk_init();
  board_init();
  configure_console();

  xSemaphoreBut = xSemaphoreCreateBinary();
  if (xSemaphoreBut == NULL)
    printf("falha em criar o semaforo BUT \n");
		
	xSemaphoreBut1 = xSemaphoreCreateBinary();
	if (xSemaphoreBut1 == NULL)
	printf("falha em criar o semaforo BUT1 \n");
		
	
  xQueueLedFreq = xQueueCreate(32, sizeof(uint32_t));
  if (xQueueLedFreq == NULL)
    printf("falha em criar a queue \n");
		
	xQueueButPress = xQueueCreate(32, sizeof(uint32_t));
	if (xQueueLedFreq == NULL)
		printf("falha em criar a queue \n");

  if (xTaskCreate(task_led, "Led", TASK_LED_STACK_SIZE, NULL,
                  TASK_LED_STACK_PRIORITY, NULL) != pdPASS) {
    printf("Failed to create test led task\r\n");
  }

  if (xTaskCreate(task_but, "BUT", TASK_BUT_STACK_SIZE, NULL,
                  TASK_BUT_STACK_PRIORITY, NULL) != pdPASS) {
    printf("Failed to create UartTx task\r\n");
  }


  vTaskStartScheduler();


  while (1) {
  }

  /* Will only get here if there was insufficient memory to create the idle
   * task. */
  return 0;
}
