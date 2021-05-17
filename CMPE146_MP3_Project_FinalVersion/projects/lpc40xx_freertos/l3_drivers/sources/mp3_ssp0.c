#include "mp3_ssp0.h"
// #include "gpio.h"
// #include "lpc40xx.h"
// #include <stdint.h>
// #include <stdio.h>
// #include <stdbool.h>
// #include <stddef.h>
// #include "clock.h"
// #include "lpc40xx.h"
// #include "lpc_peripherals.h"
// #include "mp3_decoder.h"

static void init_clock(uint32_t max_clock_khz) {
  uint8_t divider = 2;
  const uint32_t cpu_clock_khz = clock__get_peripheral_clock_hz();

  // Keep scaling down divider until calculated is higher
  while (max_clock_khz < (cpu_clock_khz / divider) && divider <= 254) {
    divider += 2; // 2 + 2 and you get the even divider of 4
  }
  // fprintf(stderr, "INITIALIZING CLOCK: %d\t", divider);
  // printf("Master Clcok Speed: %d\n", divider);
  LPC_SSP0->CPSR = divider;
}

void ssp0__init(uint32_t max_clock_mhz) {
  lpc_peripheral__turn_on_power_to(LPC_PERIPHERAL__SSP0); // Power on Peripheral

  LPC_SSP0->CR0 |= (7 << 0); // 8bit transfer
  LPC_SSP0->CR1 |= (1 << 1); // Enable the SSP

  init_clock(max_clock_mhz);
}

uint8_t ssp0__exchange_byte(uint8_t data_out) {
  // Configure the Data register(DR) to send and receive data by checking the SPI peripheral status register
  LPC_SSP0->DR = data_out;

  while (LPC_SSP0->SR & (1 << 4)) { // checks the SPI register
    ;                               // wait for SSP to complete the transfer
  }

  return (uint8_t)(LPC_SSP0->DR & 0xFF);
}

void decoder__send_data(uint16_t address) {
  (void)ssp0__exchange_byte((address >> 8) & 0xFF);
  (void)ssp0__exchange_byte((address >> 0) & 0xFF);
}

void decoder__init_clock(uint32_t clock) {
  uint8_t divider = 2;
  uint32_t clk_peripheral = clock__get_peripheral_clock_hz();
  while (clock < (clk_peripheral / divider) && divider <= 254) {
    divider += 2;
  }
  printf("Decoder Clcok Speed: %d\n", divider);
  LPC_SSP0->CPSR = divider;
}