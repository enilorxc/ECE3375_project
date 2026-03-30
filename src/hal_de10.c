#include "hal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* DE10-Standard Computer memory map from ECE3375 labs. */
#define LEDR_BASE       0xFF200000U
#define HEX3_HEX0_BASE  0xFF200020U
#define HEX5_HEX4_BASE  0xFF200030U
#define SW_BASE         0xFF200040U
#define KEY_BASE        0xFF200050U
#define TIMER0_BASE     0xFF202000U

#define MMIO32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

/* Interval timer registers (offsets). */
#define TIMER_STATUS(base)  MMIO32((base) + 0U)
#define TIMER_CONTROL(base) MMIO32((base) + 4U)
#define TIMER_PERIODL(base) MMIO32((base) + 8U)
#define TIMER_PERIODH(base) MMIO32((base) + 12U)

/* Timer control bits. */
#define TIMER_CTL_ITO   (1U << 0)
#define TIMER_CTL_CONT  (1U << 1)
#define TIMER_CTL_START (1U << 2)

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
    /* Assume 100 MHz timer clock => 100000 cycles per ms. */
    uint32_t period = 100000U - 1U;
    TIMER_CONTROL(TIMER0_BASE) = 0U;
    TIMER_PERIODL(TIMER0_BASE) = period & 0xFFFFU;
    TIMER_PERIODH(TIMER0_BASE) = (period >> 16U) & 0xFFFFU;
    TIMER_STATUS(TIMER0_BASE) = 0U;
    TIMER_CONTROL(TIMER0_BASE) = TIMER_CTL_ITO | TIMER_CTL_CONT | TIMER_CTL_START;
}

static void wait_1ms_tick(void) {
    while ((TIMER_STATUS(TIMER0_BASE) & 0x1U) == 0U) {
    }
    TIMER_STATUS(TIMER0_BASE) = 0U;
}

static void update_clock(uint32_t elapsed_ms) {
    uint32_t speed = (MMIO32(SW_BASE) & 0x1U) ? 15U : 1U;
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

void hal_init(void) {
    timer0_init_1ms();
    MMIO32(LEDR_BASE) = 0U;
    MMIO32(HEX3_HEX0_BASE) = 0U;
    MMIO32(HEX5_HEX4_BASE) = 0U;
    display_time_hhmm(g_second_of_day);
    printf("DE10 HAL initialized. SW0=1 enables 15x time speed.\n");
}

uint32_t hal_read_second_of_day(void) {
    return g_second_of_day;
}

bool hal_read_ack_button(void) {
    bool pressed = (MMIO32(KEY_BASE) & 0x1U) == 0U; /* KEY0 is active-low. */
    bool edge = pressed && !g_prev_pressed;
    g_prev_pressed = pressed;
    return edge;
}

void hal_apply_output(med_output_t output) {
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
        ledr |= 1U << 0U;
    }
    if (output.buzzer_on) {
        ledr |= 1U << 1U;
    }
    MMIO32(LEDR_BASE) = ledr;
    display_time_hhmm(g_second_of_day);

    if (!same) {
        g_last_output = output;
        g_output_valid = true;
        if (output.buzzer_on) {
            write_hex5_4_raw(g_hex_lut[0xAU], g_hex_lut[(output.buzzer_freq_hz >= 1800U) ? 0xEU : 0x1U]);
        } else {
            write_hex5_4_raw(0U, 0U);
        }
    }
}

void hal_log_event(med_event_t event) {
    if (event.type == MED_EVENT_NONE) {
        return;
    }

    uint8_t code = 0U;
    if (event.type == MED_EVENT_TAKEN_ON_TIME) {
        code = 0x0U;
    } else if (event.type == MED_EVENT_TAKEN_LATE) {
        code = 0x2U;
    } else {
        code = 0xFU;
    }
    write_hex5_4_raw(g_hex_lut[code], g_hex_lut[(uint8_t)(event.slot_index & 0xFU)]);
    printf("Event type=%d slot=%u at %lu s\n", (int)event.type, (unsigned)event.slot_index,
           (unsigned long)event.event_second_of_day);
}

void hal_delay_ms(uint32_t delay_ms) {
    for (uint32_t i = 0; i < delay_ms; ++i) {
        wait_1ms_tick();
    }
    update_clock(delay_ms);
}
