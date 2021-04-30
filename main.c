#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "board_io.h"
#include "common_macros.h"
#include "periodic_scheduler.h"
#include "sj2_cli.h"

#include "queue.h"
#include "semphr.h"

#include "acceleration.h"
#include "ff.h"
#include "gpio.h"
#include "lpc40xx.h"

#include "mp3_decoder.h"
#include "mp3_ssp0.h"
#include <stdlib.h>
#include <time.h>
// #include "mp3_reader_task.h"

// 'static' to make these functions 'private' to this file
static void create_blinky_tasks(void);
static void create_uart_task(void);
static void blink_task(void *params);
static void uart_task(void *params);

//****************************************************Lab Milestone_1*******************************************
// #if(0)

/** ***********************
 * Global
 *  ***********************
 * **/
QueueHandle_t Q_songname;
QueueHandle_t Q_songdata;

typedef char songname_t[16];
typedef char songdata_t[512];
typedef uint8_t file_buffer_t[512];
typedef uint8_t byte_512_t[512];

FIL file; // declare name
FRESULT result;

/** ***********************
 * READER RELATED FUNCTIONS
 *  ***********************
 * **/
static void mp3_reader_task(void *p) {
  songname_t name = {}; // typedef array to read song name
  songdata_t data;
  file_buffer_t file_buffer;
  UINT br;

  while (1) {
    xQueueReceive(Q_songname, &name[0], portMAX_DELAY);
    printf("Received song to play: %s\n", name);

    result = f_open(&file, name, FA_READ); // open file to read songname
    if (FR_OK == result) {
      // while (f_gets(file_buffer, sizeof(file_buffer), &file)) { // while reading from file
      //   xQueueSend(Q_songdata, &file_buffer[0], portMAX_DELAY);
      // }
      while (f_eof(&file) == 0) {
        f_read(&file, data, sizeof(data), &br);
        xQueueSend(Q_songdata, &data[0], portMAX_DELAY);
      }
      printf("Successfully opened file %s!\n", name);
    } else {
      printf("ERROR: File failed to open: %s\n", name);
    }
    f_close(&file); // close the intended file
  }
}

/** ***********************
 * PLAYER RELATED FUNCTIONS
 *  ***********************
 * **/
static void mp3_player_task(void *p) {
  file_buffer_t file_buffer;
  // int iCharWritten = 0;

  while (1) {
    xQueueReceive(Q_songdata, &file_buffer[0], portMAX_DELAY);
    for (int i = 0; i < sizeof(file_buffer); i++) {
      while (!mp3_decoder_needs_data()) {
        vTaskDelay(1);
      }
      // iCharWritten = file_buffer[i];
      // fprintf(stderr, "%02x\n", iCharWritten);
      spi_send_to_mp3_decoder(file_buffer[i]);
    }
  }
}

/** ***********************
 * Main.c
 *  ***********************
 * **/
int main(void) {
  create_blinky_tasks();
  create_uart_task();

  boot_decoder();

  Q_songname = xQueueCreate(1, sizeof(songname_t));
  Q_songdata = xQueueCreate(2, sizeof(songdata_t));

  xTaskCreate(mp3_reader_task, "reader_task", (2024 * 4) / sizeof(void *), NULL, PRIORITY_LOW, NULL);
  xTaskCreate(mp3_player_task, "player_task", (3096 * 4) / sizeof(void *), NULL, PRIORITY_LOW, NULL);
  // xTaskCreate(spi_send_to_mp3_decoder, "decoder", 4096 / sizeof(void *), NULL, PRIORITY_HIGH, NULL);

  puts("Starting RTOS");
  vTaskStartScheduler(); // This function never returns unless RTOS scheduler runs out of memory and fails

  return 0;
}
// #endif

//****************************************************Lab Milestone_1*******************************************
static void create_blinky_tasks(void) {
  /**
   * Use '#if (1)' if you wish to observe how two tasks can blink LEDs
   * Use '#if (0)' if you wish to use the 'periodic_scheduler.h' that will spawn 4 periodic tasks, one for each LED
   */
#if (1)
  // These variables should not go out of scope because the 'blink_task' will reference this memory
  static gpio_s led0, led1;

  led0 = board_io__get_led0();
  led1 = board_io__get_led1();

  xTaskCreate(blink_task, "led0", configMINIMAL_STACK_SIZE, (void *)&led0, PRIORITY_LOW, NULL);
  xTaskCreate(blink_task, "led1", configMINIMAL_STACK_SIZE, (void *)&led1, PRIORITY_LOW, NULL);
#else
  const bool run_1000hz = true;
  const size_t stack_size_bytes = 2048 / sizeof(void *); // RTOS stack size is in terms of 32-bits for ARM M4 32-bit CPU
  periodic_scheduler__initialize(stack_size_bytes, !run_1000hz); // Assuming we do not need the high rate 1000Hz task
  UNUSED(blink_task);
#endif
}

static void create_uart_task(void) {
  // It is advised to either run the uart_task, or the SJ2 command-line (CLI), but not both
  // Change '#if (0)' to '#if (1)' and vice versa to try it out
#if (0)
  // printf() takes more stack space, size this tasks' stack higher
  xTaskCreate(uart_task, "uart", (512U * 8) / sizeof(void *), NULL, PRIORITY_LOW, NULL);
#else
  sj2_cli__init();
  UNUSED(uart_task); // uart_task is un-used in if we are doing cli init()
#endif
}

static void blink_task(void *params) {
  const gpio_s led = *((gpio_s *)params); // Parameter was input while calling xTaskCreate()

  // Warning: This task starts with very minimal stack, so do not use printf() API here to avoid stack overflow
  while (true) {
    gpio__toggle(led);
    vTaskDelay(500);
  }
}

// This sends periodic messages over printf() which uses system_calls.c to send them to UART0
static void uart_task(void *params) {
  TickType_t previous_tick = 0;
  TickType_t ticks = 0;

  while (true) {
    // This loop will repeat at precise task delay, even if the logic below takes variable amount of ticks
    vTaskDelayUntil(&previous_tick, 2000);

    /* Calls to fprintf(stderr, ...) uses polled UART driver, so this entire output will be fully
     * sent out before this function returns. See system_calls.c for actual implementation.
     *
     * Use this style print for:
     *  - Interrupts because you cannot use printf() inside an ISR
     *    This is because regular printf() leads down to xQueueSend() that might block
     *    but you cannot block inside an ISR hence the system might crash
     *  - During debugging in case system crashes before all output of printf() is sent
     */
    ticks = xTaskGetTickCount();
    fprintf(stderr, "%u: This is a polled version of printf used for debugging ... finished in", (unsigned)ticks);
    fprintf(stderr, " %lu ticks\n", (xTaskGetTickCount() - ticks));

    /* This deposits data to an outgoing queue and doesn't block the CPU
     * Data will be sent later, but this function would return earlier
     */
    ticks = xTaskGetTickCount();
    printf("This is a more efficient printf ... finished in");
    printf(" %lu ticks\n\n", (xTaskGetTickCount() - ticks));
  }
}
