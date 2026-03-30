#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* =========================
 * DE10 Memory Map (ECE3375)
 * ========================= */
#define LEDR_BASE       0xFF200000U
#define HEX3_HEX0_BASE  0xFF200020U
#define HEX5_HEX4_BASE  0xFF200030U
#define SW_BASE         0xFF200040U
#define KEY_BASE        0xFF200050U
#define TIMER0_BASE     0xFF202000U

#define MMIO32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

#define TIMER_STATUS(base)  MMIO32((base) + 0U)
#define TIMER_CONTROL(base) MMIO32((base) + 4U)
#define TIMER_PERIODL(base) MMIO32((base) + 8U)
#define TIMER_PERIODH(base) MMIO32((base) + 12U)

#define TIMER_CTL_ITO   (1U << 0)
#define TIMER_CTL_CONT  (1U << 1)
#define TIMER_CTL_START (1U << 2)

/* =========================
 * Scheduler Types
 * ========================= */
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

/* =========================
 * Scheduler Implementation
 * ========================= */
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

static void med_scheduler_init(void) {
    for (size_t i = 0; i < MED_MAX_SLOTS; ++i) {
        g_ctx.slots[i].slot.enabled = false;
        g_ctx.slots[i].triggered_today = false;
    }
    g_ctx.state = STATE_IDLE;
    g_ctx.active = false;
    g_ctx.active_slot = 0U;
    g_ctx.due_time_s = 0U;
    g_ctx.pending_event.type = MED_EVENT_NONE;
    g_ctx.pending_event.slot_index = 0U;
    g_ctx.pending_event.event_second_of_day = 0U;
    g_ctx.last_second_of_day = 0U;
    set_output_idle();
}

static bool med_scheduler_set_slot(size_t index, med_slot_t slot) {
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

static void med_scheduler_step(uint32_t second_of_day, bool ack_pressed) {
    maybe_roll_day(second_of_day);

    if (g_ctx.active) {
        med_slot_t active_slot = g_ctx.slots[g_ctx.active_slot].slot;
        uint32_t elapsed_s =
            (second_of_day >= g_ctx.due_time_s) ? (second_of_day - g_ctx.due_time_s) : 0U;

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

static med_output_t med_scheduler_get_output(void) {
    return g_ctx.output;
}

static med_event_t med_scheduler_consume_event(void) {
    med_event_t out = g_ctx.pending_event;
    g_ctx.pending_event.type = MED_EVENT_NONE;
    return out;
}

/* =========================
 * DE10 HAL
 * ========================= */
static const uint8_t g_hex_lut[16] = {
    0x3F, 0x06, 0x5B, 0x4F,
    0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C,
    0x39, 0x5E, 0x79, 0x71
};

static uint32_t g_millis = 0U;
static uint32_t g_second_of_day = 8U * 3600U + 59U * 60U + 45U;
static bool g_prev_pressed = false;
static med_output_t g_last_output;
static bool g_output_valid = false;

static void write_hex4(uint32_t value) {
    uint32_t packed = 0U;
    for (uint32_t i = 0U; i < 4U; ++i) {
        uint8_t nibble = (uint8_t)((value >> (4U * i)) & 0xFU);
        packed |= (uint32_t)g_hex_lut[nibble] << (8U * i);
    }
    MMIO32(HEX3_HEX0_BASE) = packed;
}

static void write_hex5_4_raw(uint8_t hex5, uint8_t hex4) {
    uint32_t packed = ((uint32_t)hex5 << 8U) | (uint32_t)hex4;
    MMIO32(HEX5_HEX4_BASE) = packed;
}

static void timer0_init_1ms(void) {
    /* 100 MHz timer clock => 100000 cycles per ms */
    uint32_t period = 100000U - 1U;
    TIMER_CONTROL(TIMER0_BASE) = 0U;
    TIMER_PERIODL(TIMER0_BASE) = period & 0xFFFFU;
    TIMER_PERIODH(TIMER0_BASE) = (period >> 16U) & 0xFFFFU;
    TIMER_STATUS(TIMER0_BASE) = 0U;
    TIMER_CONTROL(TIMER0_BASE) = TIMER_CTL_ITO | TIMER_CTL_CONT | TIMER_CTL_START;
}

static void wait_1ms_tick(void) {
    while ((TIMER_STATUS(TIMER0_BASE) & 0x1U) == 0U) { }
    TIMER_STATUS(TIMER0_BASE) = 0U;
}

static void update_clock(uint32_t elapsed_ms) {
    uint32_t speed = (MMIO32(SW_BASE) & 0x1U) ? 15U : 1U;  /* SW0 fast mode (15x) */
    g_millis += elapsed_ms * speed;
    while (g_millis >= 1000U) {
        g_millis -= 1000U;
        g_second_of_day++;
        if (g_second_of_day >= 24U * 3600U) {
            g_second_of_day = 0U;
        }
    }
}

static void display_time_hhmm(uint32_t second_of_day) {
    uint32_t hh = second_of_day / 3600U;
    uint32_t mm = (second_of_day % 3600U) / 60U;
    uint32_t bcd = ((hh / 10U) << 12U) | ((hh % 10U) << 8U) | ((mm / 10U) << 4U) | (mm % 10U);
    write_hex4(bcd);
}

static void hal_init(void) {
    timer0_init_1ms();
    MMIO32(LEDR_BASE) = 0U;
    MMIO32(HEX3_HEX0_BASE) = 0U;
    MMIO32(HEX5_HEX4_BASE) = 0U;
    display_time_hhmm(g_second_of_day);
}

static uint32_t hal_read_second_of_day(void) {
    return g_second_of_day;
}

static bool hal_read_ack_button(void) {
    bool pressed = (MMIO32(KEY_BASE) & 0x1U) == 0U; /* KEY0 active-low */
    bool edge = pressed && !g_prev_pressed;
    g_prev_pressed = pressed;
    return edge;
}

static void hal_apply_output(med_output_t output) {
    bool same = g_output_valid &&
                output.led_on == g_last_output.led_on &&
                output.led_blink == g_last_output.led_blink &&
                output.led_blink_period_ms == g_last_output.led_blink_period_ms &&
                output.buzzer_on == g_last_output.buzzer_on &&
                output.buzzer_freq_hz == g_last_output.buzzer_freq_hz;

    bool led_visual = false;
    if (output.led_on) {
        if (output.led_blink && output.led_blink_period_ms > 0U) {
            uint32_t phase = g_millis % output.led_blink_period_ms;
            led_visual = phase < (output.led_blink_period_ms / 2U);
        } else {
            led_visual = true;
        }
    }

    uint32_t ledr = 0U;
    if (led_visual) {
        ledr |= 1U << 0U;  /* LEDR0 visual alert */
    }
    if (output.buzzer_on) {
        ledr |= 1U << 1U;  /* LEDR1 buzzer-active indicator */
    }
    MMIO32(LEDR_BASE) = ledr;
    display_time_hhmm(g_second_of_day);

    if (!same) {
        g_last_output = output;
        g_output_valid = true;
        if (output.buzzer_on) {
            /* HEX5='A', HEX4='1' normal or 'E' escalated */
            write_hex5_4_raw(g_hex_lut[0xAU], g_hex_lut[(output.buzzer_freq_hz >= 1800U) ? 0xEU : 0x1U]);
        } else {
            write_hex5_4_raw(0U, 0U);
        }
    }
}

static void hal_log_event(med_event_t event) {
    if (event.type == MED_EVENT_NONE) return;

    /* HEX5: event code, HEX4: slot index */
    uint8_t code = 0U;
    if (event.type == MED_EVENT_TAKEN_ON_TIME) code = 0x0U;
    else if (event.type == MED_EVENT_TAKEN_LATE) code = 0x2U;
    else code = 0xFU; /* missed */

    write_hex5_4_raw(g_hex_lut[code], g_hex_lut[(uint8_t)(event.slot_index & 0xFU)]);
}

static void hal_delay_ms(uint32_t delay_ms) {
    for (uint32_t i = 0; i < delay_ms; ++i) {
        wait_1ms_tick();
    }
    update_clock(delay_ms);
}

/* =========================
 * App
 * ========================= */
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

    /* More realistic demo schedule with larger response windows. */
    med_slot_t first = slot_from_second(add_minutes(now_s, 2U), 180U, 600U);
    med_slot_t second = slot_from_second(add_minutes(now_s, 5U), 180U, 600U);

    (void)med_scheduler_set_slot(0U, first);
    (void)med_scheduler_set_slot(1U, second);
}

int main(void) {
    hal_init();
    med_scheduler_init();
    configure_schedule();

    uint32_t last_second = 0xFFFFFFFFU;

    while (1) {
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
    }
}