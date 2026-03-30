#include <stdbool.h>
#include <stdio.h>

#include "hal.h"
#include "med_scheduler.h"

static uint32_t add_minutes(uint32_t second_of_day, uint32_t minutes) {
    uint32_t day_s = 24U * 3600U;
    uint32_t add_s = minutes * 60U;
    return (second_of_day + add_s) % day_s;
}

static med_slot_t slot_from_second(uint32_t second_of_day, uint16_t escalate_s, uint16_t miss_s) {
    med_slot_t slot;
    slot.hour = (uint8_t)(second_of_day / 3600U);
    slot.minute = (uint8_t)((second_of_day % 3600U) / 60U);
    slot.escalate_after_s = escalate_s;
    slot.miss_after_s = miss_s;
    slot.enabled = true;
    return slot;
}

static void configure_schedule(void) {
    uint32_t now_s = hal_read_second_of_day();
    med_slot_t first = slot_from_second(add_minutes(now_s, 2U), 180U, 600U);
    med_slot_t second = slot_from_second(add_minutes(now_s, 5U), 180U, 600U);

    if (!med_scheduler_set_slot(0U, first) || !med_scheduler_set_slot(1U, second)) {
        printf("Schedule configuration failed\n");
    }
}

int main(void) {
    hal_init();
    med_scheduler_init();
    configure_schedule();

    uint32_t last_second = 0xFFFFFFFFU;
#ifdef TARGET_SIM
    uint32_t start_second = hal_read_second_of_day();
#endif

    for (;;) {
        uint32_t now_s = hal_read_second_of_day();
        bool ack_pressed = hal_read_ack_button();

        if (now_s != last_second) {
            med_scheduler_step(now_s, ack_pressed);
            last_second = now_s;
        } else if (ack_pressed) {
            med_scheduler_step(now_s, true);
        }

        hal_apply_output(med_scheduler_get_output());
        hal_log_event(med_scheduler_consume_event());
        hal_delay_ms(50U);

#ifdef TARGET_SIM
        uint32_t elapsed =
            (now_s >= start_second) ? (now_s - start_second) : (24U * 3600U - start_second + now_s);
        if (elapsed >= 5U * 60U) {
            break;
        }
#endif
    }

    return 0;
}
