#include "mp3_decoder.h"

// /************************************************************
//  *                  DECODER to BOARD Setup                  *
//  * ------------------------SPI/SSP0------------------------ *
//  * -> SCLK-0                         to P0_15 (J5 5)        *
//  * -> MISO-0                         to P0_17 (J6 4)        *
//  * -> MOSI-0                         to P0_18 (J5 4)        *
//  *                                                          *
//  * -> CS (active low)                to P2_1 (J6 10)        *
//  *                                                          *
//  * -> DREQ (Data request, input bus) to P2_0 (J5 10)        *
//  * -> RST  (Active Low Reset)        to P2_4 (J6 9)         *
//  * -> XDCS (Data Chip Select)        to P2_2 (J5 9)         *
//  *                                                          *
//  * ------------------------Audio------------------------    *
//  * -> LOUT (Left channel output)     to L port              *
//  * -> ROUT (Right channel output)    to R port              *
//  * -> AGND (Analog ground)           to gnd port            *
//  *                                                          *
//  *************************************************************/

void setup_pins(void) {
  uint8_t GPIO_pin_function_reset_mask = (7 << 0);

  LPC_IOCON->P0_15 &= ~GPIO_pin_function_reset_mask;
  gpio__construct_with_function(0, 15, GPIO__FUNCTION_2); // SCLK-0

  LPC_IOCON->P0_17 &= ~GPIO_pin_function_reset_mask;
  gpio__construct_with_function(0, 17, GPIO__FUNCTION_2); // MISO-0

  LPC_IOCON->P0_18 &= ~GPIO_pin_function_reset_mask;
  gpio__construct_with_function(0, 18, GPIO__FUNCTION_2); // M0SI-0

  gpio__construct_with_function(2, 1, 0); // CS
  gpio__construct_as_output(2, 1);

  gpio__construct_with_function(2, 0, 0); // DREQ
  gpio__construct_as_input(2, 0);

  gpio__construct_with_function(2, 4, 0); // RST
  gpio__construct_as_output(2, 4);

  gpio__construct_with_function(2, 2, 0); // XDCS
  gpio__construct_as_output(2, 2);
}

void cs_activate(void) { // Turn on at 0
  LPC_GPIO2->CLR = (1 << 1);
}

void cs_deactivate(void) { // Turn off at 1
  LPC_GPIO2->SET = (1 << 1);
}

bool dreq_get_read_status(void) {
  bool val;
  val = LPC_GPIO2->PIN & (1 << 0);
  return val;
}

bool mp3_decoder_needs_data(void) { return dreq_get_read_status(); }

void rst_activate(void) { // Turn on at 0
  LPC_GPIO2->CLR = (1 << 4);
}

void rst_deactivate(void) { // Turn off at 1
  LPC_GPIO2->SET = (1 << 4);
}

void xdcs_activate(void) { // Turn on at 0
  LPC_GPIO2->CLR = (1 << 2);
}

void xdcs_deactivate(void) { // Turn off at 1
  LPC_GPIO2->SET = (1 << 2);
}

// /************************************************************
//  *                  SPI * SCI R/W FUNCTIONS                 *
//  * ------------------------  Write  ----------------------- *
//  * -> How it works:                                         *
//  *    1. Activate CS                                        *
//  *    2. Write OpCode transfer byte: 0x02 via SI            *
//  *    3. Write the 8-bit Adress                             *
//  *    4. Write the 16-bit Data-out                          *
//  *    5. Check DREQ                                         *
//  *    6. Deactivate CS                                      *
//  * ------------------------  READ   ----------------------- *
//  *
//  * ------------------------  START  ----------------------- *
//  *                                                          *
//  *************************************************************/

//------------------------  Send to Decoder   -----------------------
void spi_send_to_mp3_decoder(uint8_t file_buffer) {
  while (!dreq_get_read_status()) {
    ;
  }
  xdcs_activate();                  // Select
  ssp0__exchange_byte(file_buffer); // Send SPI Data
  xdcs_deactivate();                // Deselect
}

// //------------------------  Write   -----------------------
void sci_write(uint8_t address, uint16_t data_out) {
  cs_activate();
  ssp0__exchange_byte(op_write);
  ssp0__exchange_byte(address);
  decoder__send_data(data_out);
  cs_deactivate();
}

//------------------------  READ   -----------------------
uint16_t sci_read(uint8_t address) {
  uint16_t data1, data2, data;

  cs_activate();

  ssp0__exchange_byte(op_read);
  ssp0__exchange_byte(address);
  data1 = ssp0__exchange_byte(0xFF);
  data2 = ssp0__exchange_byte(0xFF);

  data = (data1 << 8) | data2;

  cs_deactivate();

  return data;
}

//------------------------  PRINT DECODER STATES  -----------------------

void print_states(void) {
  uint16_t MP3Status = sci_read(SCI_STATUS); // Status
  uint16_t vsVersion = (MP3Status >> 4) & 0x000F;
  printf("Version of VS1053: %d\n", vsVersion);

  uint16_t MP3_CLK = sci_read(SCI_CLOCKF); // Clock
  printf("SCK: %x\n", MP3_CLK);
  // delay__ms(100);

  uint16_t mp3_mode = sci_read(SCI_MODE); // Mode
  printf("MODE: %x\n", mp3_mode);
}

//------------------------  START   -----------------------
void boot_decoder(void) {
  ssp0__init(1);
  setup_pins(); // Configure Pins
  hard_reset();
  print_states();

  decoder__init_clock(4000000);
  uint16_t MP3_CLK_3 = sci_read(SCI_CLOCKF);
  printf("SCK_3: %x\n", MP3_CLK_3);
  // ssp0__init(3 * 1000 * 1000);
}

//------------------------  VOLUME   -----------------------

void set_volume(uint16_t volume) {
  sci_write(SCI_VOL, volume);
  uint16_t mp3_volume = sci_read(SCI_VOL);
  printf("Volume: %x\n", mp3_volume);
}

//------------------------  INITIALIZE DECODER   -----------------------

void soft_reset(void) {
  sci_write(SCI_MODE, (0x0800 | 0x0004));
  delay__ms(10);
}

void hard_reset(void) {
  rst_activate();
  delay__ms(10);
  rst_deactivate();

  ssp0__init(3 * 1000 * 1000);
  // ssp0__exchange_byte(0xFF);

  cs_deactivate();
  xdcs_deactivate();

  delay__ms(10);
  soft_reset();
  delay__ms(10);

  sci_write(SCI_CLOCKF, 0x6000);

  set_volume(0x0101);

  // sci_write(SCI_CLOCKF, 0x6000);
  // delay__ms(10);
}