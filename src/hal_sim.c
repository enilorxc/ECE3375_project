#include "hal.h"

#include <stdio.h>
#include <time.h>

static uint32_t g_second_of_day = 8U * 3600U + 59U * 60U + 45U;
static bool g_ack_latched = false;
static med_output_t g_last_output;
static bool g_output_initialized = false;

static void print_time(uint32_t sec) {
    uint32_t hh = sec / 3600U;
    uint32_t mm = (sec % 3600U) / 60U;
    uint32_t ss = sec % 60U;
    printf("[%02u:%02u:%02u]", hh, mm, ss);
}

void hal_init(void) {
    printf("HAL SIM initialized\n");
}

uint32_t hal_read_second_of_day(void) {
    if (g_second_of_day >= 24U * 3600U - 1U) {
        g_second_of_day = 0U;
    } else {
        g_second_of_day++;
    }
    return g_second_of_day;
}

bool hal_read_ack_button(void) {
    if (g_second_of_day == (9U * 3600U + 1U * 60U + 10U) && !g_ack_latched) {
        g_ack_latched = true;
        return true;
    }
    return false;
}

void hal_apply_output(med_output_t output) {
    if (g_output_initialized &&
        output.led_on == g_last_output.led_on &&
        output.led_blink == g_last_output.led_blink &&
        output.led_blink_period_ms == g_last_output.led_blink_period_ms &&
        output.buzzer_on == g_last_output.buzzer_on &&
        output.buzzer_freq_hz == g_last_output.buzzer_freq_hz) {
        return;
    }
    g_last_output = output;
    g_output_initialized = true;

    print_time(g_second_of_day);
    printf(" LED:%s", output.led_on ? (output.led_blink ? "BLINK" : "ON") : "OFF");
    if (output.led_blink) {
        printf("(%ums)", output.led_blink_period_ms);
    }
    printf(" BUZZER:%s", output.buzzer_on ? "ON" : "OFF");
    if (output.buzzer_on) {
        printf("(%uHz)", output.buzzer_freq_hz);
    }
    printf("\n");
}

void hal_log_event(med_event_t event) {
    if (event.type == MED_EVENT_NONE) {
        return;
    }

    print_time(event.event_second_of_day);
    printf(" SLOT:%zu EVENT:", event.slot_index);
    switch (event.type) {
        case MED_EVENT_TAKEN_ON_TIME:
            printf("TAKEN_ON_TIME");
            break;
        case MED_EVENT_TAKEN_LATE:
            printf("TAKEN_LATE");
            break;
        case MED_EVENT_MISSED:
            printf("MISSED");
            break;
        default:
            printf("UNKNOWN");
            break;
    }
    printf("\n");
}

void hal_delay_ms(uint32_t delay_ms) {
    struct timespec req;
    req.tv_sec = (time_t)(delay_ms / 1000U);
    req.tv_nsec = (long)((delay_ms % 1000U) * 1000000U);
    nanosleep(&req, NULL);
}
