#include "mp3_oled.h"

#include "lpc40xx.h"
#include "lpc_peripherals.h"

#include "delay.h"
#include "gpio.h"
#include "string.h"

#include "mp3_ssp0.h"

/**********************************************************
 *         OLED PIN CONFIGURATIONS
 * -> MOSI-1 to P0_9
 * -> SCK-1  to P0_7
 * -> _DC    to P1_25
 **********************************************************/
void pin_setup_oled(void) {
  gpio__construct_with_function(0, 9, GPIO__FUNCTION_2);
  gpio__construct_with_function(0, 7, GPIO__FUNCTION_2);
  gpio__construct_with_function(1, 25, 0);

  gpio__construct_as_output(0, 9);
  gpio__construct_as_output(0, 7);
  gpio__construct_as_output(1, 25);

  gpio__construct_as_output(1, 22);
}

void cs_activate_oled(void) { LPC_GPIO1->PIN &= ~(1 << 22); }

void cs_deactivate_oled(void) { LPC_GPIO1->PIN |= (1 << 22); }

void toggle_dc_cmd(void) { LPC_GPIO1->PIN &= ~(1 << 25); }

void toggle_dc_data(void) { LPC_GPIO1->PIN |= (1 << 25); }

void init_oled(void) {
  lpc_peripheral__turn_on_power_to(LPC_PERIPHERAL__SSP1); // Power on peripheral

  LPC_SSP1->CR0 |= (7 << 0);
  LPC_SSP1->CR1 |= (1 << 1); // set up control registers

  // Keep scaling down divider until calculated is higher
  // set prescalar register
  uint32_t oled_clk = 8 * 1000 * 1000;
  uint8_t divider = 2;
  const uint32_t cpu_clock_khz = clock__get_peripheral_clock_hz();
  while (oled_clk < (cpu_clock_khz / divider) && divider <= 254) {
    divider += 2; // 2 + 2 and you get the even divider of 4
  }
  // fprintf(stderr, "INITIALIZING CLOCK: %d\t", divider);
  LPC_SSP1->CPSR = divider;
}

uint8_t ssp1__exchange_byte(uint8_t data_out) {
  // Configure the Data register(DR) to send and receive data by checking the SPI peripheral status register
  LPC_SSP1->DR = data_out;

  while (LPC_SSP1->SR & (1 << 4)) { // checks the SPI register
    ;                               // wait for SSP to complete the transfer
  }

  return (uint8_t)(LPC_SSP1->DR & 0xFF);
}

void update_oled() {
  horizontal_addr_mode();
  for (int row = 0; row < 8; row++) {
    for (int column = 0; column < 128; column++) {
      toggle_dc_data();
      ssp1__exchange_byte(bitmap_[row][column]);
    }
  }
}

// /**********************************************************
//  *        OLED DISPAY INITIALIZATION
//  * -> Refer to section 4.4 of OLED Display Datasheet (pg 15)
//  * -> Turn the Data/Command control pin to LOW
//  *    -> Data will be transferred to cmd register
//  *    -> Refer to figure 7-1: Pin Dispcription (pg 15/64)
//  *
//  **********************************************************/
void display_cmd_oled(void) {
  toggle_dc_cmd();

  //------------------------  Set Display Off  -----------------------
  ssp1__exchange_byte(0xAE);

  //------------------------  Set Display Clock Divide Ratio/Oscillator Frequency  -----------------------
  ssp1__exchange_byte(0xD5);
  ssp1__exchange_byte(0x80);

  //------------------------  Set Multiplex Ratio  -----------------------
  ssp1__exchange_byte(0xA8);
  ssp1__exchange_byte(0x3F);

  //------------------------  Set Display Offset  -----------------------
  ssp1__exchange_byte(0xD3);
  ssp1__exchange_byte(0x00);

  //------------------------  Set Display Start Line  -----------------------
  ssp1__exchange_byte(0x40);

  //------------------------  Set Charge Pump  -----------------------
  ssp1__exchange_byte(0x8D);
  ssp1__exchange_byte(0x14);

  //------------------------  Set Segment Re-Map  -----------------------
  ssp1__exchange_byte(0xA1);

  //------------------------  Set COM Output Scan Direction  -----------------------
  ssp1__exchange_byte(0xC8);

  //------------------------  Set COM Pins Hardware Configuration  -----------------------
  ssp1__exchange_byte(0xDA);
  ssp1__exchange_byte(0x12);

  //------------------------  Set Contrast Control  -----------------------
  ssp1__exchange_byte(0x81);
  ssp1__exchange_byte(0xCF);

  //------------------------  Set Pre-Charge Period -----------------------
  ssp1__exchange_byte(0xD9);
  ssp1__exchange_byte(0xF1);

  //------------------------  Set VCOMH Deselect Level -----------------------
  ssp1__exchange_byte(0xDB);
  ssp1__exchange_byte(0x40);

  // Horizontal or vertical address mode inserted here?
  horizontal_addr_mode();

  //------------------------  Set Entire Display On/Off -----------------------
  ssp1__exchange_byte(0xA4);

  //------------------------  Set Normal/Inverse Display -----------------------
  ssp1__exchange_byte(0xA6);

  //------------------------  Set Display On -----------------------
  ssp1__exchange_byte(0xAF);
}

/**********************************************************
 *                   ALPHABET, #S, & MORE
 * -> Refer to section 8.7 of SSD1306 (pg 26) for info
 *    -> Refer to FONT GEN Excel sheet for array setup
 *    -> Chose font size of 5x7
 * -> Use function pointer to store address of fxns.
 *    -> return & argument type is void
 *    -> *func_pointer is the pointer to a funtion
 **********************************************************/

// typedef void (*func_pointer)(void);
static func_pointer show_char_on_oled[128] = {};

//  function clear the entire a screen
void clear_oled(void) { memset(bitmap_, 0x00, sizeof(bitmap_)); }

// function fill the entire a screen
void fill() { memset(bitmap_, 0xFF, sizeof(bitmap_)); }

/**********************************************************
 *                   POWER UP SEQUENCE
 * -> Refer to section 4.2 of OLED Display Datasheet (pg 14)
 *    -> Refer to "Power Up Sequence" section
 *
 **********************************************************/
void turn_on_oled(void) {
  pin_setup_oled();
  init_oled();
  clear_oled();

  cs_activate_oled();

  delay__ms(100);
  display_cmd_oled();
  array_setup_oled();

  cs_deactivate_oled();
}

/**********************************************************
 *                   HORIZONTAL_ADDR_MODE
 * -> Refer to Addressing Setting Command Table
 *    of OLED Display Datasheet (pg 33)
 *
 **********************************************************/

void horizontal_addr_mode() {
  toggle_dc_cmd();
  ssp1__exchange_byte(0x20); // SET memory addr mode
  ssp1__exchange_byte(0x00); // dummy byte

  // ----------------------------- The column address ----------------------------
  ssp1__exchange_byte(0x21);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x7F);

  // ---------------------------- The page address ----------------------------
  ssp1__exchange_byte(0x22);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x07);
}

void set_page_start(oled page_number_oled) {
  cs_activate_oled();
  toggle_dc_cmd();
  ssp1__exchange_byte(0xB0 | page_number_oled);
  cs_deactivate_oled();
}

/**********************************************************
 *                   SCREEN DISPLAY FUN
 * -> Refer to CH 9
 * -> REFER TO Addressing Setting Command Table
 *    of OLED Display Datasheet (pg 33)
 * -> Refer to chapter 9 of command table for ssd1306 datasheet
 *
 **********************************************************/
void invert_mode(oled page) {
  cs_activate_oled();
  toggle_dc_cmd();
  ssp1__exchange_byte(0xB0 | page);
  ssp1__exchange_byte(0xA7);
  cs_deactivate_oled();
}

void horizontal_scrolling(oled oled_page) {
  cs_activate_oled();
  toggle_dc_cmd();
  ssp1__exchange_byte(0x27); // sscroll left
  ssp1__exchange_byte(0x00); // dummy byte
  ssp1__exchange_byte(0x00 | oled_page);
  ssp1__exchange_byte(0x07); // interval between each scroll step
  ssp1__exchange_byte(0x00 | oled_page);
  ssp1__exchange_byte(0x00); // dummy byte
  ssp1__exchange_byte(0xFF); // dummy byte
  ssp1__exchange_byte(0x2F); // activate scrolling
  cs_deactivate_oled();
}

void vertical_scrolling_r(oled oled_page) {
  cs_activate_oled();
  toggle_dc_cmd();
  ssp1__exchange_byte(0x29); // Vertical and Right Horizontal Scroll
  ssp1__exchange_byte(0x00); // dummy byte
  ssp1__exchange_byte(0x00 | oled_page);
  ssp1__exchange_byte(0x07);
  ssp1__exchange_byte(0x00 | oled_page);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x2F); // activate scrolling
  cs_deactivate_oled();
}

void vertical_scrolling_l(oled oled_page) {
  cs_activate_oled();
  toggle_dc_cmd();
  ssp1__exchange_byte(0x2A); // Vertical and Right Horizontal Scroll
  ssp1__exchange_byte(0x00); // dummy byte
  ssp1__exchange_byte(0x00 | oled_page);
  ssp1__exchange_byte(0x07);
  ssp1__exchange_byte(0x00 | oled_page);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x2F); // activate scrolling
  cs_deactivate_oled();
}

/*After sending 2Eh command to deactivate
the scrolling action, the ram data needs
to be rewritten.*/

void deactive_scroll() {
  cs_activate_oled();
  toggle_dc_cmd();
  ssp1__exchange_byte(0x2E); // DEACTIVATE SCROLL
  cs_deactivate_oled();
}

/* Refer to page 37 to set new page and stuff*/
void new_line(oled oled_page) {
  toggle_dc_cmd();
  ssp1__exchange_byte(0xB0 | oled_page);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x10); // page addressing mode
  toggle_dc_data();
}

/**********************************************************
 *                   PRINT DISPLAY PAGES
 * -> Refer to CH 9
 * -> REFER TO Addressing Setting Command Table
 *    of OLED Display Datasheet (pg 33)
 * -> Refer to chapter 9 of command table for ssd1306 datasheet
 *
 **********************************************************/
void OLED_PRINT(char *str, oled oled_page) {
  cs_activate_oled();
  toggle_dc_cmd();
  ssp1__exchange_byte(0xB0 | oled_page);
  ssp1__exchange_byte(0x10);
  ssp1__exchange_byte(0x00);
  toggle_dc_data();
  for (int i = 0; i < strlen(str); i++) {
    // if (str[i] == '\n') {
    //   cursor++;
    //   new_line(cursor);
    //   continue;
    // }
    function_pointer_oled handle = show_char_on_oled[(int)(str[i])];
    handle();
  }
  cs_deactivate_oled();
}

void Erase_oled(oled page_number, oled_Erase_oled single_or_all) {
  cs_activate_oled();
  toggle_dc_cmd();

  ssp1__exchange_byte(0xB0); // setting column pointer
  ssp1__exchange_byte(0x10);
  ssp1__exchange_byte(0x00);
  if (single_or_all) {
    cs_activate_oled();
    toggle_dc_data();
    for (int i = 0; i < 128; i++) {
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);

      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
    }
  } else {
    cs_activate_oled();
    toggle_dc_data();
    for (int i = 0; i < 16; i++) {
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);

      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
      ssp1__exchange_byte(0x00);
    }
  }
  cs_deactivate_oled();
}

void array_setup_oled(void) {
  show_char_on_oled[(int)'A'] = show_A;
  show_char_on_oled[(int)'B'] = show_B;
  show_char_on_oled[(int)'C'] = show_C;
  show_char_on_oled[(int)'D'] = show_D; // a lil baddie
  show_char_on_oled[(int)'E'] = show_E;
  show_char_on_oled[(int)'F'] = show_F;
  show_char_on_oled[(int)'G'] = show_G;
  show_char_on_oled[(int)'H'] = show_H;
  show_char_on_oled[(int)'I'] = show_I;
  show_char_on_oled[(int)'J'] = show_J;
  show_char_on_oled[(int)'K'] = show_K;
  show_char_on_oled[(int)'L'] = show_L;
  show_char_on_oled[(int)'M'] = show_M;
  show_char_on_oled[(int)'N'] = show_N;
  show_char_on_oled[(int)'O'] = show_O;
  show_char_on_oled[(int)'P'] = show_P;
  show_char_on_oled[(int)'Q'] = show_Q;
  show_char_on_oled[(int)'R'] = show_R;
  show_char_on_oled[(int)'S'] = show_S;
  show_char_on_oled[(int)'T'] = show_T;
  show_char_on_oled[(int)'U'] = show_U;
  show_char_on_oled[(int)'V'] = show_V;
  show_char_on_oled[(int)'W'] = show_W;
  show_char_on_oled[(int)'X'] = show_X;
  show_char_on_oled[(int)'Y'] = show_Y;
  show_char_on_oled[(int)'Z'] = show_Z;

  show_char_on_oled[(int)'a'] = show_a;
  show_char_on_oled[(int)'b'] = show_b;
  show_char_on_oled[(int)'c'] = show_c;
  show_char_on_oled[(int)'d'] = show_d;
  show_char_on_oled[(int)'e'] = show_e;
  show_char_on_oled[(int)'f'] = show_f;
  show_char_on_oled[(int)'g'] = show_g;
  show_char_on_oled[(int)'h'] = show_h;
  show_char_on_oled[(int)'i'] = show_i;
  show_char_on_oled[(int)'j'] = show_j;
  show_char_on_oled[(int)'k'] = show_k;
  show_char_on_oled[(int)'l'] = show_l;
  show_char_on_oled[(int)'m'] = show_m;
  show_char_on_oled[(int)'n'] = show_n;
  show_char_on_oled[(int)'o'] = show_o;
  show_char_on_oled[(int)'p'] = show_p;
  show_char_on_oled[(int)'q'] = show_q;
  show_char_on_oled[(int)'r'] = show_r;
  show_char_on_oled[(int)'s'] = show_s;
  show_char_on_oled[(int)'t'] = show_t; // my lil boo thang
  show_char_on_oled[(int)'u'] = show_u;
  show_char_on_oled[(int)'v'] = show_v;
  show_char_on_oled[(int)'w'] = show_w;
  show_char_on_oled[(int)'x'] = show_x;
  show_char_on_oled[(int)'y'] = show_y;
  show_char_on_oled[(int)'z'] = show_z;

  show_char_on_oled[(int)'0'] = show_0;
  show_char_on_oled[(int)'1'] = show_1;
  show_char_on_oled[(int)'2'] = show_2;
  show_char_on_oled[(int)'3'] = show_3;
  show_char_on_oled[(int)'4'] = show_4;
  show_char_on_oled[(int)'5'] = show_5;
  show_char_on_oled[(int)'6'] = show_6;
  show_char_on_oled[(int)'7'] = show_7;
  show_char_on_oled[(int)'8'] = show_8;
  show_char_on_oled[(int)'9'] = show_9;

  show_char_on_oled[(int)'!'] = show_explanation;
  show_char_on_oled[(int)'@'] = show_at;
  show_char_on_oled[(int)'#'] = show_hash;
  show_char_on_oled[(int)'$'] = show_dolla;
  show_char_on_oled[(int)'%'] = show_percent;
  show_char_on_oled[(int)'^'] = show_carrot_top;
  show_char_on_oled[(int)'&'] = show_and;
  show_char_on_oled[(int)'*'] = show_star;

  show_char_on_oled[(int)'_'] = show_underline;
  show_char_on_oled[(int)'-'] = show_minus;
  show_char_on_oled[(int)'+'] = show_plus;
  show_char_on_oled[(int)'='] = show_equal;

  show_char_on_oled[(int)'{'] = show_l_curly;
  show_char_on_oled[(int)'}'] = show_r_curly;
  show_char_on_oled[(int)'['] = show_l_braket;
  show_char_on_oled[(int)']'] = show_r_braket;
  show_char_on_oled[(int)'('] = show_l_parentheses;
  show_char_on_oled[(int)')'] = show_r_parentheses;

  show_char_on_oled[(int)'|'] = show_bar;
  show_char_on_oled[(int)'\\'] = show_back_slashes;
  show_char_on_oled[(int)'/'] = show_forward_slash;
  show_char_on_oled[(int)':'] = show_colon;
  show_char_on_oled[(int)';'] = show_semicolon;
  show_char_on_oled[(int)'"'] = show_d_qoute;
  show_char_on_oled[(int)'\''] = show_s_qoute;
  show_char_on_oled[(int)'?'] = show_question_mark;
  show_char_on_oled[(int)'`'] = show_b_mark;

  show_char_on_oled[(int)'<'] = show_less_than;
  show_char_on_oled[(int)'>'] = show_greater_than;
  show_char_on_oled[(int)','] = show_comma;
  show_char_on_oled[(int)'.'] = show_period;
  show_char_on_oled[(int)'~'] = show_tilde;
  show_char_on_oled[(int)' '] = show_space;
  show_char_on_oled[128] = show_square1;
  show_char_on_oled[129] = show_square_2;
  show_char_on_oled[130] = show_square_3;
  show_char_on_oled[131] = show_square_fullsize;
}

void show_b_mark(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x02);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_A(void) {
  ssp1__exchange_byte(0x7E);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x7E);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_B(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x36);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_C(void) {
  ssp1__exchange_byte(0x3E);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x22);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_D(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x3E);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_E(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_F(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_G(void) {
  ssp1__exchange_byte(0x3E);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x3A);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_H(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_I(void) {
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_J(void) {
  ssp1__exchange_byte(0x20);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x3F);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_K(void) {
  ssp1__exchange_byte(0x7F); // hey
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x22);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_L(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_M(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x02);
  ssp1__exchange_byte(0x0C);
  ssp1__exchange_byte(0x02);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}
void show_N(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x02);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x7F); // hey
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_O(void) {
  ssp1__exchange_byte(0x3E); // hey
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x3E); // hey
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_P(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x06);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_Q(void) {
  ssp1__exchange_byte(0x3E);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x51);
  ssp1__exchange_byte(0x21);
  ssp1__exchange_byte(0x5E);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_R(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x19);
  ssp1__exchange_byte(0x29);
  ssp1__exchange_byte(0x46);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_S(void) {
  ssp1__exchange_byte(0x26);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x32);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_T(void) {
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_U(void) {
  ssp1__exchange_byte(0x3F);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x3F);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_V(void) {
  ssp1__exchange_byte(0x1F);
  ssp1__exchange_byte(0x20);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x20);
  ssp1__exchange_byte(0x1F);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_W(void) {
  ssp1__exchange_byte(0x3F);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x38);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x3F);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_X(void) {
  ssp1__exchange_byte(0x63);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x63);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_Y(void) {
  ssp1__exchange_byte(0x07);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x70);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x07);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_Z(void) {
  ssp1__exchange_byte(0x61);
  ssp1__exchange_byte(0x51);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x45);
  ssp1__exchange_byte(0x43);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_a(void) {
  ssp1__exchange_byte(0x20);
  ssp1__exchange_byte(0x54);
  ssp1__exchange_byte(0x54);
  ssp1__exchange_byte(0x54);
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_b(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x38);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_c(void) {
  ssp1__exchange_byte(0x38);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_d(void) {
  ssp1__exchange_byte(0x38);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_e(void) {
  ssp1__exchange_byte(0x38);
  ssp1__exchange_byte(0x54);
  ssp1__exchange_byte(0x54);
  ssp1__exchange_byte(0x54);
  ssp1__exchange_byte(0x18);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_f(void) {
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x7E);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x02);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_g(void) {
  ssp1__exchange_byte(0x18);
  ssp1__exchange_byte(0xA4);
  ssp1__exchange_byte(0xA4);
  ssp1__exchange_byte(0xA4);
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_h(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_i(void) {
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x7D);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_j(void) {
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x80);
  ssp1__exchange_byte(0x80);
  ssp1__exchange_byte(0x84);
  ssp1__exchange_byte(0x7D);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_k(void) {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x10);
  ssp1__exchange_byte(0x28);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_l(void) {
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_m(void) {
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x18);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_n(void) {
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_o(void) {
  ssp1__exchange_byte(0x38);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x38);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_p(void) {
  ssp1__exchange_byte(0xFC);
  ssp1__exchange_byte(0x24);
  ssp1__exchange_byte(0x24);
  ssp1__exchange_byte(0x24);
  ssp1__exchange_byte(0x18);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_q(void) {
  ssp1__exchange_byte(0x18);
  ssp1__exchange_byte(0x24);
  ssp1__exchange_byte(0x24);
  ssp1__exchange_byte(0x28);
  ssp1__exchange_byte(0xFC);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_r(void) {
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_s(void) {
  ssp1__exchange_byte(0x48);
  ssp1__exchange_byte(0x54);
  ssp1__exchange_byte(0x54);
  ssp1__exchange_byte(0x54);
  ssp1__exchange_byte(0x20);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_t(void) {
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x3E);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x20);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_u(void) {
  ssp1__exchange_byte(0x3C);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x20);
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_v(void) {
  ssp1__exchange_byte(0x1C);
  ssp1__exchange_byte(0x20);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x20);
  ssp1__exchange_byte(0x1C);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_w(void) {
  ssp1__exchange_byte(0x3C); // hey
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x30);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x3C);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_x(void) {
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x28);
  ssp1__exchange_byte(0x10);
  ssp1__exchange_byte(0x28);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_y(void) {
  ssp1__exchange_byte(0x9C);
  ssp1__exchange_byte(0xA0);
  ssp1__exchange_byte(0xA0);
  ssp1__exchange_byte(0xA0);
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}
void show_z(void) {
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x64);
  ssp1__exchange_byte(0x54);
  ssp1__exchange_byte(0x4C);
  ssp1__exchange_byte(0x44);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_0(void) { // check
  ssp1__exchange_byte(0x3E);
  ssp1__exchange_byte(0x51);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x45);
  ssp1__exchange_byte(0x3E);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_1(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x42);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x40);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_2(void) { // check
  ssp1__exchange_byte(0x42);
  ssp1__exchange_byte(0x61);
  ssp1__exchange_byte(0x51);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x46);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_3(void) { // check
  ssp1__exchange_byte(0x22);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x36);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_4(void) { // check
  ssp1__exchange_byte(0x18);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x12);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x10);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_5(void) { // check
  ssp1__exchange_byte(0x27);
  ssp1__exchange_byte(0x45);
  ssp1__exchange_byte(0x45);
  ssp1__exchange_byte(0x45);
  ssp1__exchange_byte(0x39);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_6(void) { // check
  ssp1__exchange_byte(0x3C);
  ssp1__exchange_byte(0x4A);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x30);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_7(void) { // check
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x71);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x05);
  ssp1__exchange_byte(0x03);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_8(void) { // check
  ssp1__exchange_byte(0x36);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x36);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_9(void) { // check
  ssp1__exchange_byte(0x06);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x29);
  ssp1__exchange_byte(0x1E);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_explanation(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x5F);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_at(void) { // check
  ssp1__exchange_byte(0x32);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x79);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x3E);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_hash(void) { // check
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_dolla(void) { // check
  ssp1__exchange_byte(0x24);
  ssp1__exchange_byte(0x2A);
  ssp1__exchange_byte(0x6B);
  ssp1__exchange_byte(0x2A);
  ssp1__exchange_byte(0x12);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_percent(void) { // check
  ssp1__exchange_byte(0x23);
  ssp1__exchange_byte(0x13);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x64);
  ssp1__exchange_byte(0x62);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_carrot_top(void) { // check
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x02);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x02);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_and(void) { // check
  ssp1__exchange_byte(0x36);
  ssp1__exchange_byte(0x49);
  ssp1__exchange_byte(0x55);
  ssp1__exchange_byte(0x22);
  ssp1__exchange_byte(0x50);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_star(void) { // check
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x3E);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_underline(void) { // check
  ssp1__exchange_byte(0x80);
  ssp1__exchange_byte(0x80);
  ssp1__exchange_byte(0x80);
  ssp1__exchange_byte(0x80);
  ssp1__exchange_byte(0x80);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_minus(void) { // check
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_plus(void) { // check
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x3E);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_equal(void) { // check
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_l_curly(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x36);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_r_curly(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x36);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_l_braket(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_r_braket(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_l_parentheses(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x1C);
  ssp1__exchange_byte(0x22);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_r_parentheses(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x22);
  ssp1__exchange_byte(0x1C);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_bar(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_back_slashes(void) { // check
  ssp1__exchange_byte(0x02);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x16);
  ssp1__exchange_byte(0x32);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_forward_slash(void) { // check
  ssp1__exchange_byte(0x20);
  ssp1__exchange_byte(0x10);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x02);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_colon(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x36);
  ssp1__exchange_byte(0x36);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_semicolon(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x56);
  ssp1__exchange_byte(0x36);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_d_qoute(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x07);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x07);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_s_qoute(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x05);
  ssp1__exchange_byte(0x03);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_question_mark(void) { // check
  ssp1__exchange_byte(0x02);
  ssp1__exchange_byte(0x01);
  ssp1__exchange_byte(0x51);
  ssp1__exchange_byte(0x09);
  ssp1__exchange_byte(0x06);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_less_than(void) { // check
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x22);
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_greater_than(void) {
  ssp1__exchange_byte(0x41);
  ssp1__exchange_byte(0x22);
  ssp1__exchange_byte(0x14);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_comma(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0xA0);
  ssp1__exchange_byte(0x60);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_period(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x60);
  ssp1__exchange_byte(0x60);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_tilde(void) { // check
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x08);
  ssp1__exchange_byte(0x04);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_space(void) { // check
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
  ssp1__exchange_byte(0x00);
}

void show_square_3() {
  ssp1__exchange_byte(0x7E);
  ssp1__exchange_byte(0x7E);
  ssp1__exchange_byte(0x7E);
  ssp1__exchange_byte(0x7E);
  ssp1__exchange_byte(0x7E);
  ssp1__exchange_byte(0x7E);
  ssp1__exchange_byte(0x7E);
  ssp1__exchange_byte(0x00);
}
void show_square_2() {
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x7C);
  ssp1__exchange_byte(0x00);
}
void show_square1() {
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x78);
  ssp1__exchange_byte(0x00);
}
void show_square_fullsize() {
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x7F);
  ssp1__exchange_byte(0x00);
}