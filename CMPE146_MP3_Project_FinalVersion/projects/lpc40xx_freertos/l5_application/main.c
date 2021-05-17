#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "board_io.h"
#include "common_macros.h"
#include "periodic_scheduler.h"
#include "sj2_cli.h"

#include "lpc_peripherals.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "acceleration.h"
#include "ff.h"
#include "gpio.h"
#include "lpc40xx.h"

#include "mp3_decoder.h"
#include "mp3_isr.h"
#include "mp3_oled.h"
#include "mp3_song_list.h"
#include "mp3_ssp0.h"

#include <stdlib.h>
#include <time.h>

// // 'static' to make these functions 'private' to this file
// static void create_blinky_tasks(void);
// static void create_uart_task(void);
// static void blink_task(void *params);
// static void uart_task(void *params);

//++++++++++++++++++++++++++++++++++++++++++++++++++++   Lab Milestone_1
//++++++++++++++++++++++++++++++++++++++++++++++++++++

// #if(0)

/**  **********************************************
 *                     Global
 ** **********************************************/

QueueHandle_t Q_songname;
QueueHandle_t Q_songdata;

static SemaphoreHandle_t pause_play__song_switch_press;
SemaphoreHandle_t go_down;
SemaphoreHandle_t play_next;
SemaphoreHandle_t go_play;

TaskHandle_t play_song;

typedef char songname_t[32];
typedef char songdata_t[512];
typedef uint8_t file_buffer_t[512];
typedef uint8_t byte_512_t[512];

songname_t name; // typedef array to read song name
songdata_t data;
byte_512_t byte_512;
UINT br;

char song_meta[128];
typedef struct {
  char song_Tag[3];
  char song_Title[30];
  char song_Artist[30];
  char song_Album[30];
  uint8_t comment;
  char song_Year[4];
} meta_data;

uint8_t cursor_forscrolling = 0;
uint16_t mouse = 0;

uint8_t current_volume = 3;

volatile bool pause = true;
uint16_t next = 0;
uint16_t previous = 0;

FIL file; // declare name
FRESULT result;



volatile bool press = true;

/** **********************************************
 *                    FUNCTIONS
 ** **********************************************/
// ------------------------   DISPLAYS   ------------------------
void loading_screen(void);
void display_menu(void);
void oled_playlist();
void display_music_settings();

void display_paused();
void display_playing();

void display_get_song_meta_data();

// ------------------------   META   ------------------------
void get_song_meta_data(char *song_meta);

// ------------------------   TASKS   ------------------------
static void mp3_reader_task(void *p);
static void mp3_player_task(void *p);
static void mp3_pause_task(void *p);

void mp3_vol_up_task(void *p);
void mp3_vol_down_task(void *p);

void mp3_playlist_task(void *p);
void click_detection(void *p);

// ------------------------   INTERRUPTS   ------------------------
void pause_song_isr(void);
void do_something_isr(void);
void playlist_isr(void);

void go_down_isr(void);
void play_next_isr(void);

void configure_your_gpio_interrupt(void);

// ------------------------   OTHER   ------------------------

/** **********************************************
 *          DISPLAY RELATED FUNCTIONS
 ** **********************************************/
void loading_screen() {
  Erase_oled(0, ALL_PAGES);
  OLED_PRINT("WELCOME :)", page3);
  vertical_scrolling_r(page3);
  delay__ms(2000);
  Erase_oled(0, ALL_PAGES);

  Erase_oled(0, ALL_PAGES);
  OLED_PRINT("YOUR PLAYLIST", page3);
  horizontal_scrolling(page3);
  delay__ms(2000);
  Erase_oled(0, ALL_PAGES);
  deactive_scroll();
  // display("ABCDEFGHIJ\n");
  // display("KLMNOPQRST\n");
  // display("UVWXYZ\n");
  // display("abcdefghij\n");
  // display("klmnopqrst\n");
  // display("uvwxyz\n");
  // display("0123456789\n");
  // display("!@#$%^&*()\n");
  // display("~_-+={}[]\\|\n");
  // display(":;<>,.?/\n");
}

void oled_playlist() {
  uint8_t oled_page_counter = 0;
  Erase_oled(0, ALL_PAGES);
  for (int i = mouse; i < mouse + 8; i++) {
    if (i == song_list__get_item_count()) {
      break;
    }
    char *song = song_list__get_name_for_item(i);
    OLED_PRINT(song, oled_page_counter);
    oled_page_counter++;
  }
}

void display_playing() {
  Erase_oled(0, ALL_PAGES);
  OLED_PRINT("Playing", page0);
  OLED_PRINT(&name, page2);
  //   OLED_PRINT(current_volume, page7);
  vTaskResume(play_song);
  delay__ms(100);
}

void display_paused() {
  Erase_oled(0, ALL_PAGES);

  OLED_PRINT("||", page0);
  OLED_PRINT(&name, page2);
  //   OLED_PRINT(current_volume, page7);
  vTaskSuspend(play_song);
  delay__ms(100);
}

/** **********************************************
 *          READER RELATED FUNCTIONS
 ** **********************************************/
static void mp3_reader_task(void *p) {
  while (1) {
    if (xQueueReceive(Q_songname, &name, portMAX_DELAY)) {
      // vTaskSuspend(play_song);
      printf("Received song to play: %s\n", name);
      //-----------------------------------   open file to read songname + Meta   -----------------------------------
      result = f_open(&file, name, FA_READ);

      f_lseek(&file, f_size(&file) - (sizeof(char) * 128));
      f_read(&file, song_meta, sizeof(song_meta), &br);
      f_lseek(&file, 0);

      display_get_song_meta_data();

      if (FR_OK == result) {
        printf("Opened the song to play: %s\n", name);
        f_read(&file, data, sizeof(data), &br);
        printf("read the song to play: %s\n", name);
        // f_eof(&file) == 0
        while (br != 0) {
          // fprintf(stderr, "while the song to play: %s\n", name);
          f_read(&file, data, sizeof(data), &br);
          // printf("read 23the song to play: %s\n", name);
          xQueueSend(Q_songdata, data, portMAX_DELAY);
          if (uxQueueMessagesWaiting(Q_songname)) {
            printf("New play song request\n");
            break;
          }
        }
        printf("Successfully opened file %s!\n", name);
      }
      if (br == 0) {
        xSemaphoreGive(go_play);
      } else {
        printf("ERROR: File failed to open: %s\n", name);
      }
      f_close(&file); // close the intended file
    }
  }
}

/**  **********************************************
 *           PLAYER RELATED FUNCTIONS
 ** **********************************************/
static void mp3_player_task(void *p) {
  file_buffer_t file_buffer;
  // int iCharWritten = 0;

  while (1) {
    xQueueReceive(Q_songdata, &file_buffer, portMAX_DELAY);
    // fprintf(stderr, "player task the song to play:\n");
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

/** **********************************************
 *          META DATA RELATED FUNCTIONS
 ** **********************************************/
// get song_meta_data
void get_song_meta_data(char *song_meta) {
  meta_data song_meta_data = {};
  for (int i = 0; i < 128; i++) {
    if ((((int)(song_meta[i]) > 3) && ((int)(song_meta[i]) < 128))) {
      char c = (int)(song_meta[i]);
      if (i > 0 && i < 3) {
        song_meta_data.song_Tag[i] = c;
      } else if (i < 30) {
        song_meta_data.song_Title[i - 3] = c;
      } else if (i < 60) {
        song_meta_data.song_Artist[i - 33] = c;
      } else if (i < 90) {
        song_meta_data.song_Album[i - 63] = c;
      } else if (i < 97) {
        song_meta_data.song_Year[i - 93] = c;
      } else if (i == 127) {
        song_meta_data.comment = (int)(song_meta[i]);
      }
    }
  }
  OLED_PRINT("TITLE:", page0);
  OLED_PRINT(song_meta_data.song_Title, page1);
  OLED_PRINT("Artist:", page2);
  OLED_PRINT(song_meta_data.song_Artist, page3);
  OLED_PRINT("Year:", page4);
  OLED_PRINT(song_meta_data.song_Year, page5);
  OLED_PRINT("Album", page6);
  OLED_PRINT(song_meta_data.song_Album, page7);
}

void display_get_song_meta_data() {
  Erase_oled(0, ALL_PAGES);
  get_song_meta_data(song_meta);
}
/** **********************************************
 *          PAUSE/PLAY RELATED FUNCTIONS
 ** **********************************************/
void next_song_display(void) {
  if (mouse = song_list__get_item_count()) {
    next = 0;
  } else {
    next = mouse++;
  }
}

void prev_song_display(void) {
  if (mouse == 0) {
    previous = song_list__get_item_count();
  } else {
    previous = mouse--;
  }
}

static void mp3_pause_task(void *p) {
  while (1) {
    if (xSemaphoreTake(pause_play__song_switch_press, portMAX_DELAY)) {
      if (pause) {
        display_paused();
        pause = false;
      } else {
        display_playing();
        pause = true;
      }
    }
  }
}

/** **********************************************
 *          PLAY ON BOARD RELATED FUNCTIONS
 ** **********************************************/

/** **********************************************
 *          VOLUME RELATED FUNCTIONS
 ** **********************************************/
uint16_t volume_settings[5] = {0xFEFE, 0x4B4B, 0x3535, 0x2525, 0x0101};
void mp3_vol_up_task(void *p) {
  while (1) {
    if (LPC_GPIO1->PIN & (1 << 15)) {
      if (current_volume == 5) {
        current_volume = 3;
      }
      current_volume++;
      set_volume(volume_settings[current_volume]);
      mp3_volume_display();
    }
  }
}

void mp3_vol_down_task(void *p) {
  while (1) {
    if (LPC_GPIO1->PIN & (1 << 19)) {
      if (current_volume == 0) {
        current_volume = 3;
      }
      current_volume--;
      set_volume(volume_settings[current_volume]);
      mp3_volume_display();
    }
  }
}

void mp3_volume_display() {
  Erase_oled(0, ALL_PAGES);
  OLED_PRINT("volume: ", page4);
  for (int i = 0; i < current_volume; i++) {
    cs_activate_oled();
    toggle_dc_cmd();
    toggle_dc_data();
    if (i < 1) {
      show_square1();
    } else if (i < 3) {
      show_square_2();
    } else if (i < 5) {
      show_square_3();
    } else {
      show_square_fullsize();
    }
    cs_deactivate_oled();
  }
}
/** **********************************************
 *        PLAY NEXT SONG RELATED FUNCTIONS
 ** **********************************************/
void mp3_next_song_task(void *p) {
  while (1) {
    if (xSemaphoreTake(go_play, portMAX_DELAY)) {
      if (press) {
        press = false;
        char *song = song_list__get_name_for_item(mouse);
        mouse++;
        xQueueSend(Q_songname, song, portMAX_DELAY);
      }
    }
    vTaskDelay(100);
  }
}

/** **********************************************
 *        View playlist and cursor
 ** **********************************************/
void mp3_playlist_task(void *p) {
  while (1) {
    if (xSemaphoreTake(go_down, portMAX_DELAY)) {
      if (cursor_forscrolling == song_list__get_item_count()) {
        // display_get_song_meta_data();
        cursor_forscrolling = 0;
        oled_playlist();
      }
      if (cursor_forscrolling != 0) {
        // deactive_scroll();
        //   Erase_oled(cursor_forscrolling, SINGLE_PAGES);
        OLED_PRINT(song_list__get_name_for_item(mouse), cursor_forscrolling);
        fprintf(stderr, "cursor in go down: %i\t", cursor_forscrolling);
        fprintf(stderr, "Mouse in go down: %i\n", mouse);
      }
      horizontal_scrolling(cursor_forscrolling);
      cursor_forscrolling++;
      if (mouse > song_list__get_item_count()) {
        mouse = 0;
        cursor_forscrolling = 0;
        Erase_oled(0, ALL_PAGES);
        oled_playlist();
        fprintf(stderr, "we are in the over scrolling part");
      } else {
        mouse++;
        fprintf(stderr, "Mouse in go doiuhwn: %i\n", mouse);
      }
    }
  }
}

/** **********************************************
 *          BUTTON RELATED FUNCTIONS
 ** **********************************************/
void mp3_click(void *p) {
  while (1) {
    if (xSemaphoreTake(play_next, portMAX_DELAY)) {
      fprintf(stderr, "Mouse kygkygkgkgjbjhb go doiuhwn: %i\n", mouse);
      vTaskResume(play_next);
      char *song = song_list__get_name_for_item(mouse - 1);
      xQueueSend(Q_songname, song, portMAX_DELAY);
    }
    vTaskDelay(500);
  }
}

/** **********************************************
 *          INTERRUPT RELATED FUNCTIONS
 ** **********************************************/
void pause_song_isr(void) { xSemaphoreGiveFromISR(pause_play__song_switch_press, NULL); }

void go_down_isr(void) { xSemaphoreGiveFromISR(go_down, NULL); }

void play_next_isr(void) { xSemaphoreGiveFromISR(play_next, NULL); }

void do_something_isr(void) { xSemaphoreGiveFromISR(go_play, NULL); }
void configure_your_gpio_interrupt() {
  NVIC_EnableIRQ(GPIO_IRQn);
  lpc_peripheral__enable_interrupt(LPC_PERIPHERAL__GPIO, gpio0__interrupt_dispatcher, "unused");
  gpio0__attach_interrupt(6, GPIO_INTR__FALLING_EDGE, pause_song_isr);
  gpio0__attach_interrupt(30, GPIO_INTR__FALLING_EDGE, go_down_isr);
  gpio0__attach_interrupt(29, GPIO_INTR__FALLING_EDGE, play_next_isr);
  gpio0__attach_interrupt(8, GPIO_INTR__FALLING_EDGE, do_something_isr);
}
/** **********************************************
 * Main.c
 *  **********************************************
 * **/

int main() {
  //-----------------------------------   BUTTONS   -----------------------------------
  gpio__construct_as_input(0, 29); // SW3- Functionalities: Select Song to play
  gpio__construct_as_input(0, 30); // SW2- Functionalities: Scroll throught list
  gpio__construct_as_input(1, 15); // SW1- Functionalities: Volume control up
  gpio__construct_as_input(1, 19); // SW0- Functionalities: Volume control down
  gpio__construct_as_input(0, 6);  // YELLOW- Functionalities:
  gpio__construct_as_input(0, 8);  // GREEN-

  //-----------------------------------   POWER-UP SEQUENCE   -----------------------------------
  boot_decoder();
  turn_on_oled();

  clear_oled();
  update_oled();

  loading_screen();
  delay__ms(1000);

  song_list__populate();
  oled_playlist();
  sj2_cli__init();

  //-----------------------------------   DEFINE QUEUES AND SEMAPHORES   -----------------------------------

  Q_songname = xQueueCreate(1, sizeof(songname_t));
  Q_songdata = xQueueCreate(2, sizeof(songdata_t));

  pause_play__song_switch_press = xSemaphoreCreateBinary();

  go_down = xSemaphoreCreateBinary();
  play_next = xSemaphoreCreateBinary();

  go_play = xSemaphoreCreateBinary();

  //-----------------------------------   INTERRUPTS   -----------------------------------

  configure_your_gpio_interrupt();

  //-----------------------------------   TASKS   -----------------------------------
  xTaskCreate(mp3_reader_task, "reader_task", (2024 * 4) / sizeof(void *), NULL, PRIORITY_LOW, NULL);
  xTaskCreate(mp3_player_task, "player_task", (3096 * 4) / sizeof(void *), NULL, PRIORITY_LOW, &play_song);

  xTaskCreate(mp3_pause_task, "pause_task", 4096 / sizeof(void *), NULL, PRIORITY_LOW, NULL);
  xTaskCreate(mp3_vol_down_task, "vol_down_task", (1024) / sizeof(void *), NULL, PRIORITY_LOW, NULL);
  xTaskCreate(mp3_vol_up_task, "vol_up_task", (1024) / sizeof(void *), NULL, PRIORITY_LOW, NULL);

  xTaskCreate(mp3_playlist_task, "go_down", 2048 / sizeof(void *), NULL, PRIORITY_LOW, NULL);
  xTaskCreate(mp3_click, "click", 2048 / sizeof(void *), NULL, PRIORITY_MEDIUM, NULL);
  xTaskCreate(mp3_next_song_task, "click", 2048 / sizeof(void *), NULL, PRIORITY_LOW, NULL);

  // xTaskCreate()

  puts("Starting RTOS");
  vTaskStartScheduler(); // This function never returns unless RTOS scheduler runs out of memory and fails

  return 0;
}
// #endif

//++++++++++++++++++++++++++++++++++++++++++++++++++++   Lab Milestone_1
//++++++++++++++++++++++++++++++++++++++++++++++++++++
