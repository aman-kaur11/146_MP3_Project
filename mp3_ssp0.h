#pragma once
#include <stdint.h>
#include <stdio.h>

#include "clock.h"
#include "gpio.h"
#include "lpc40xx.h"
#include "lpc_peripherals.h"
#include <stdio.h>

/*This will initalize the registers and power on the peripheral*/
void ssp0__init(uint32_t max_clock_mhz);

/*This will configure the data register (DR) to send and receive data
  This will be done by checking the SPI peripheral status regiser*/
uint8_t ssp0__exchange_byte(uint8_t data_out);

void decoder__send_data(uint16_t address);

void decoder__init_clock(uint32_t clock);