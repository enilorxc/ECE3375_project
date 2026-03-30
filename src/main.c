#include <stdbool.h>
#include <stdio.h>

#include "hal.h"
#include "med_scheduler.h"

static void configure_schedule(void) {
    med_slot_t morning = {
        .hour = 9U,
        .minute = 0U,
        .escalate_after_s = 30U,
        .miss_after_s = 120U,
        .enabled = true,
    };
    med_slot_t evening = {
        .hour = 21U,
        .minute = 0U,
        .escalate_after_s = 20U,
        .miss_after_s = 80U,
        .enabled = true,
    };

    if (!med_scheduler_set_slot(0U, morning) || !med_scheduler_set_slot(1U, evening)) {
        printf("Schedule configuration failed\n");
    }
}

int main(void) {
    hal_init();
    med_scheduler_init();
    configure_schedule();

    /* Simulate one full day for prototyping verification. */
    for (uint32_t i = 0; i < 24U * 3600U; ++i) {
        uint32_t now_s = hal_read_second_of_day();
        bool ack_pressed = hal_read_ack_button();

        med_scheduler_step(now_s, ack_pressed);
        hal_apply_output(med_scheduler_get_output());
        hal_log_event(med_scheduler_consume_event());
        hal_delay_ms(0U);
    }

    return 0;
}
