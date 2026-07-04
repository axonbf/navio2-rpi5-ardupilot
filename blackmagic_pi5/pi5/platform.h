#ifndef __PLATFORM_H
#define __PLATFORM_H

#include <stdint.h>

#define SET_RUN_STATE(state)
#define SET_IDLE_STATE(state)
#define SET_ERROR_STATE(state)

#define PLATFORM_FATAL_ERROR(error)	abort()
#define PLATFORM_SET_FATAL_ERROR_RECOVERY()

#define GPIO_SWDCLK 12
#define GPIO_SWDIO 13

#define morse_msg 0

#define SWDIO_MODE_FLOAT() \
    gpio_direction(GPIO_SWDIO, false);
#define SWDIO_MODE_DRIVE() \
    gpio_direction(GPIO_SWDIO, true);

int platform_init(int argc, char **argv);
void morse(const char *msg, char repeat);
const char *platform_target_voltage(void);
void platform_delay(uint32_t delay);

void platform_buffer_flush(void);
int platform_buffer_write(const uint8_t *data, int size);
int platform_buffer_read(uint8_t *data, int size);

#endif
