#include "med_scheduler.h"

#include <string.h>

typedef enum {
    STATE_IDLE = 0,
    STATE_ALERT_ACTIVE,
    STATE_ALERT_ESCALATED
} alert_state_t;

typedef struct {
    med_slot_t slot;
    bool triggered_today;
} slot_state_t;

typedef struct {
    slot_state_t slots[MED_MAX_SLOTS];
    alert_state_t state;
    bool active;
    size_t active_slot;
    uint32_t due_time_s;
    med_event_t pending_event;
    med_output_t output;
    uint32_t last_second_of_day;
} scheduler_ctx_t;

static scheduler_ctx_t g_ctx;

static uint32_t slot_to_second_of_day(const med_slot_t *slot) {
    return (uint32_t)slot->hour * 3600U + (uint32_t)slot->minute * 60U;
}

static void set_output_idle(void) {
    g_ctx.output.led_on = false;
    g_ctx.output.led_blink = false;
    g_ctx.output.led_blink_period_ms = 0U;
    g_ctx.output.buzzer_on = false;
    g_ctx.output.buzzer_freq_hz = 0U;
}

static void set_output_active(void) {
    g_ctx.output.led_on = true;
    g_ctx.output.led_blink = true;
    g_ctx.output.led_blink_period_ms = 1000U;
    g_ctx.output.buzzer_on = true;
    g_ctx.output.buzzer_freq_hz = 1200U;
}

static void set_output_escalated(void) {
    g_ctx.output.led_on = true;
    g_ctx.output.led_blink = true;
    g_ctx.output.led_blink_period_ms = 300U;
    g_ctx.output.buzzer_on = true;
    g_ctx.output.buzzer_freq_hz = 2000U;
}

void med_scheduler_init(void) {
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.pending_event.type = MED_EVENT_NONE;
    set_output_idle();
}

bool med_scheduler_set_slot(size_t index, med_slot_t slot) {
    if (index >= MED_MAX_SLOTS || slot.hour > 23U || slot.minute > 59U) {
        return false;
    }
    if (slot.escalate_after_s == 0U) {
        slot.escalate_after_s = 30U;
    }
    if (slot.miss_after_s <= slot.escalate_after_s) {
        slot.miss_after_s = (uint16_t)(slot.escalate_after_s + 60U);
    }

    g_ctx.slots[index].slot = slot;
    g_ctx.slots[index].triggered_today = false;
    return true;
}

static void raise_event(med_event_type_t type, size_t slot_idx, uint32_t second_of_day) {
    g_ctx.pending_event.type = type;
    g_ctx.pending_event.slot_index = slot_idx;
    g_ctx.pending_event.event_second_of_day = second_of_day;
}

static void reset_active_alert(void) {
    g_ctx.active = false;
    g_ctx.state = STATE_IDLE;
    g_ctx.active_slot = 0U;
    g_ctx.due_time_s = 0U;
    set_output_idle();
}

static void maybe_roll_day(uint32_t second_of_day) {
    if (second_of_day < g_ctx.last_second_of_day) {
        for (size_t i = 0; i < MED_MAX_SLOTS; ++i) {
            g_ctx.slots[i].triggered_today = false;
        }
    }
    g_ctx.last_second_of_day = second_of_day;
}

void med_scheduler_step(uint32_t second_of_day, bool ack_pressed) {
    maybe_roll_day(second_of_day);

    if (g_ctx.active) {
        med_slot_t active_slot = g_ctx.slots[g_ctx.active_slot].slot;
        uint32_t elapsed_s = second_of_day >= g_ctx.due_time_s
                                 ? (second_of_day - g_ctx.due_time_s)
                                 : 0U;

        if (ack_pressed) {
            med_event_type_t type = (elapsed_s <= active_slot.escalate_after_s)
                                        ? MED_EVENT_TAKEN_ON_TIME
                                        : MED_EVENT_TAKEN_LATE;
            raise_event(type, g_ctx.active_slot, second_of_day);
            reset_active_alert();
            return;
        }

        if (elapsed_s >= active_slot.miss_after_s) {
            raise_event(MED_EVENT_MISSED, g_ctx.active_slot, second_of_day);
            reset_active_alert();
            return;
        }

        if (elapsed_s >= active_slot.escalate_after_s) {
            g_ctx.state = STATE_ALERT_ESCALATED;
            set_output_escalated();
        } else {
            g_ctx.state = STATE_ALERT_ACTIVE;
            set_output_active();
        }
        return;
    }

    for (size_t i = 0; i < MED_MAX_SLOTS; ++i) {
        med_slot_t slot = g_ctx.slots[i].slot;
        if (!slot.enabled || g_ctx.slots[i].triggered_today) {
            continue;
        }

        uint32_t slot_s = slot_to_second_of_day(&slot);
        if (second_of_day >= slot_s) {
            g_ctx.slots[i].triggered_today = true;
            g_ctx.active = true;
            g_ctx.active_slot = i;
            g_ctx.due_time_s = slot_s;
            g_ctx.state = STATE_ALERT_ACTIVE;
            set_output_active();
            return;
        }
    }
}

med_output_t med_scheduler_get_output(void) {
    return g_ctx.output;
}

med_event_t med_scheduler_consume_event(void) {
    med_event_t out = g_ctx.pending_event;
    g_ctx.pending_event.type = MED_EVENT_NONE;
    return out;
}
