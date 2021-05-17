#pragma once

#include <stdint.h>
#include <stdio.h>

uint8_t bitmap_[8][128];

typedef enum {
  page0 = 0x00,
  page1 = 0x01,
  page2 = 0x02,
  page3 = 0x03,
  page4 = 0x04,
  page5 = 0x05,
  page6 = 0x06,
  page7 = 0x07,
} oled;

typedef enum {
  SINGLE_PAGES = 0,
  ALL_PAGES,
} oled_Erase_oled;

typedef void (*function_pointer_oled)(void);

void turn_on_oled();

void pin_setup_oled();

void init_oled();

void clear_oled();

void cs_activate_oled();

void cs_deactivate_oled();

void display_cmd_oled();

void horizontal_addr_mode();

void array_setup_oled();
void OLED_PRINT(char *str, oled oled_page);
void OLED(char *str);
void fill();

void new_line(oled oled_page);
void page_begin(oled oled_page);
void update_oled();

void toggle_dc_cmd();

void toggle_dc_data();
void set_up_char_array();

void Erase_oled(oled page_number, oled_Erase_oled single_all);
void deactive_scroll();
void horizontal_scrolling(oled oled_page);
void vertical_scrolling(oled oled_page);
uint8_t ssp1__exchange_byte(uint8_t data_out);
typedef void (*func_pointer)(void);

void show_A();
void show_B();
void show_C();
void show_D();
void show_E();
void show_F();
void show_G();
void show_H();
void show_I();
void show_J();
void show_K();
void show_L();
void show_M();
void show_N();
void show_O();
void show_P();
void show_Q();
void show_R();
void show_S();
void show_T();
void show_U();
void show_V();
void show_W();
void show_X();
void show_Y();
void show_Z();

void show_a();
void show_b();
void show_c();
void show_d();
void show_e();
void show_f();
void show_g();
void show_h();
void show_i();
void show_j();
void show_k();
void show_l();
void show_m();
void show_n();
void show_o();
void show_p();
void show_q();
void show_r();
void show_s();
void show_t();
void show_u();
void show_v();
void show_w();
void show_x();
void show_y();
void show_z();

void show_0();
void show_1();
void show_2();
void show_3();
void show_4();
void show_5();
void show_6();
void show_7();
void show_8();
void show_9();

void show_explanation();
void show_at();
void show_hash();
void show_dolla();
void show_percent();
void show_carrot_top();
void show_and();
void show_star();

void show_underline();
void show_minus();
void show_plus();
void show_equal();

void show_l_curly();
void show_r_curly();
void show_l_braket();
void show_r_braket();
void show_l_parentheses();
void show_r_parentheses();

void show_bar();
void show_back_slashes();
void show_forward_slash();
void show_colon();
void show_semicolon();
void show_d_qoute();
void show_s_qoute();
void show_question_mark();

void show_less_than();
void show_greater_than();
void show_comma();
void show_period();
void show_tilde();
void show_space();
void show_b_mark();
void show_square1();
void show_square_2();
void show_square_3();
void show_square_fullsize();