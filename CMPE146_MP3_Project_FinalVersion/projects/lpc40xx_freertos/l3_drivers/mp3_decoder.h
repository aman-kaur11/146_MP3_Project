#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "clock.h"
#include "delay.h"
#include "gpio.h"
#include "lpc40xx.h"
#include "lpc_peripherals.h"

#include "mp3_ssp0.h"

/* Define Decoder Registers */
#define SCI_MODE 0x00   // Mode control
#define SCI_STATUS 0x01 // Status of VS1053b
#define SCI_CLOCKF 0x03 // Clock freq + multiplier
#define SCI_VOL 0x0B    // Volume control

#define op_read 0x03  // Read Opcode
#define op_write 0x02 // Write Opcode

// /**
//  * To Start up VS1053B:
//  * 1. Configure the pins
//  * 2.
//  *
//  */

// /*******************************************
//  *
//  *         PRIVATE FUNCTIONS
//  *
//  ********************************************/

void boot_decoder(void);

// /*******************************************
//  *
//  *         PRIVATE FUNCTIONS
//  *
//  ********************************************/

void setup_pins(void);

void cs_activate(void);
void cs_deactivate(void);

bool dreq_get_read_status(void);

void rst_deactivate(void);
void rst_activate(void);

void xdcs_activate(void);
void xdcs_deactivate(void);

void mp3_startup(void);
void decoder_send_mp3Data(uint8_t data);

void xdcs_activate(void);
void xdcs_deactivate(void);

bool mp3_decoder_needs_data(void);

void spi_send_to_mp3_decoder(uint8_t byte_512_t);

void sci_write(uint8_t address, uint16_t data_out);

uint16_t sci_read(uint8_t address);

uint8_t spi_read(void);

void soft_reset(void);

void hard_reset(void);

uint8_t begin(void);

void set_volume(uint16_t volume);

void boot_decoder(void);
