#ifndef MED_SCHEDULER_H
#define MED_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MED_MAX_SLOTS 8U

typedef enum {
    MED_EVENT_NONE = 0,
    MED_EVENT_TAKEN_ON_TIME,
    MED_EVENT_TAKEN_LATE,
    MED_EVENT_MISSED
} med_event_type_t;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint16_t escalate_after_s;
    uint16_t miss_after_s;
    bool enabled;
} med_slot_t;

typedef struct {
    bool led_on;
    bool led_blink;
    uint16_t led_blink_period_ms;
    bool buzzer_on;
    uint16_t buzzer_freq_hz;
} med_output_t;

typedef struct {
    med_event_type_t type;
    size_t slot_index;
    uint32_t event_second_of_day;
} med_event_t;

void med_scheduler_init(void);
bool med_scheduler_set_slot(size_t index, med_slot_t slot);
void med_scheduler_step(uint32_t second_of_day, bool ack_pressed);
med_output_t med_scheduler_get_output(void);
med_event_t med_scheduler_consume_event(void);

#endif
