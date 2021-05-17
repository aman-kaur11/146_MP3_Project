#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "lpc40xx.h"
#include "mp3_isr.h"

// Note: You may want another separate array for falling vs. rising edge callbacks
static function_pointer_t gpio0_callbacks[32];

void gpio0__attach_interrupt(uint32_t pin, gpio_interrupt_e interrupt_type, function_pointer_t callback) {
  // 1) Store the callback based on the pin at gpio0_callbacks
  gpio0_callbacks[pin] = callback;

  // 2) Configure GPIO 0 pin for rising or falling edge
  if (interrupt_type == GPIO_INTR__RISING_EDGE) {
    LPC_GPIOINT->IO0IntEnR |= (1 << pin);

  } else if (interrupt_type == GPIO_INTR__FALLING_EDGE) {
    LPC_GPIOINT->IO0IntEnF |= (1 << pin);
  }
  // fprintf(stderr, "Attach interrupt...\n");
}

// We wrote some of the implementation for you
void gpio0__interrupt_dispatcher(void) {
  const int logic_that_you_will_write(void);
  void clear_pin_interrupt(const int pinny);

  // Check which pin generated the interrupt
  const int pin_that_generated_interrupt = logic_that_you_will_write();
  function_pointer_t attached_user_handler = gpio0_callbacks[pin_that_generated_interrupt];

  // Invoke the user registered callback, and then clear the interrupt
  attached_user_handler();
  clear_pin_interrupt(pin_that_generated_interrupt);

  // fprintf(stderr, "...inside interrupt dispatcher\n");
}

const int logic_that_you_will_write(void) {
  int i = 0;
  int val = 0;
  for (i = 0; i < 32; i++) {
    if ((LPC_GPIOINT->IO0IntStatF & (1 << i))) {
      val = i;
    }
    if ((LPC_GPIOINT->IO0IntStatR & (1 << i))) {
      val = i;
    }
  }
  // fprintf(stderr, "retrieving pin that generated interrupt...\n");
  return val;
}

void clear_pin_interrupt(const int pin_that_generated_interrupt) {
  if (LPC_GPIOINT->IO0IntStatF) {
    LPC_GPIOINT->IO0IntClr |= (1 << pin_that_generated_interrupt);
  }
  if (LPC_GPIOINT->IO0IntStatR) {
    LPC_GPIOINT->IO0IntClr |= (1 << pin_that_generated_interrupt);
  }
  // fprintf(stderr, "Clearing the pin interrupt...\n");
}