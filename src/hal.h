#ifndef HAL_H
#define HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "med_scheduler.h"

void hal_init(void);
uint32_t hal_read_second_of_day(void);
bool hal_read_ack_button(void);
void hal_apply_output(med_output_t output);
void hal_log_event(med_event_t event);
void hal_delay_ms(uint32_t delay_ms);

#endif
