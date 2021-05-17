#include "mp3_reader_task.h"

#include "FreeRTOS.h"
#include "queue.h"

#include <stdio.h>
#include <string.h>

#include "task.h"

#include "queue.h"
#include "semphr.h"

#include "ff.h"
#include "lpc40xx.h"
#include "ssp2.h"

FIL file; // declare name
FRESULT result;

extern QueueHandle_t Q_songname;
extern QueueHandle_t Q_songdata;

typedef char songname_t[16];
typedef char songdata_t[512];
typedef char file_buffer_t[512];

/** ***********************
 * READER RELATED FUNCTIONS
 *  ***********************
 * **/
void open_file(const char *filename) {
  result = f_open(&file, filename, FA_READ); // open file to read songname
  if (FR_OK == result) {
    printf("Successfully opened file %s!\n", filename);
  } else {
    printf("ERROR: File failed to open: %s\n", filename);
  }
}

void read_file(const char *filename) {
  file_buffer_t file_buffer;  // file buffer
  while (f_eof(&file) == 0) { // while reading a string from file
    f_gets(file_buffer, sizeof(file_buffer), &file);
    xQueueSend(Q_songdata, &file_buffer[0], portMAX_DELAY);
  }
}

void close_file(const char *filename) { f_close(&file); }

void mp3_reader_task(void *p) {
  songname_t name = {}; // typedef array to read song name

  while (1) {
    xQueueReceive(Q_songname, &name[0], portMAX_DELAY);
    printf("Received song to play: %s\n", name);

    open_file(name); // open the intended file

    read_file(name);  // Queue the data to be played
    close_file(name); // close the intended file
  }
}