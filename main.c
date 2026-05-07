#include <avr/io.h>
#include <util/delay.h>
#include <string.h>

// ── SETTINGS ──────────────────────────────────────────────────

#define CODE           "PASTS"

#define REPEAT_COUNT        0
#define BLINK_ON_MS       700
#define BLINK_OFF_MS      700
#define GROUP_PAUSE_MS   1300
#define LETTER_PAUSE_MS  3000
#define WORD_PAUSE_MS    6000

// ── SIGNAL BURST TUNING ───────────────────────────────────────
// The start/end marker is 6 rapid flashes — much faster than
// any regular tap blink, so it cannot be misread as data.
// Keep BURST_ON + BURST_OFF well below BLINK_ON_MS.

#define BURST_FLASHES       6     // must be 6+ to be unambiguous
#define BURST_ON_MS        80     // very short on  — clearly "not a tap"
#define BURST_OFF_MS       80     // very short off
#define BURST_PRE_PAUSE   800     // silence before the burst
#define BURST_POST_PAUSE 1200     // silence after  the burst before data starts

// ── FADE TUNING ───────────────────────────────────────────────

#define FADE_STEPS        60

// ── POLYBIUS SQUARE ───────────────────────────────────────────

static const uint8_t POLYBIUS[26][2] = {
    {1,1},{1,2},{1,3},{1,4},{1,5},
    {2,1},{2,2},{2,3},{2,4},{2,5},
    {1,3},                          // K → C
    {3,1},{3,2},{3,3},{3,4},{3,5},
    {4,1},{4,2},{4,3},{4,4},{4,5},
    {5,1},{5,2},{5,3},{5,4},{5,5}
};

// ── PWM (pin 9 = PB1 = OC1A) ──────────────────────────────────

static void pwm_init(void) {
    DDRB   |= (1 << PB1);
    TCCR1A  = (1 << COM1A1) | (1 << WGM10);
    TCCR1B  = (1 << WGM12)  | (1 << CS10);
    OCR1AL  = 0;
}

static inline void set_brightness(uint8_t val) { OCR1AL = val; }

// ── TIMING ────────────────────────────────────────────────────

static void delay_ms(uint16_t ms) {
    for (uint16_t i = 0; i < ms; i++) _delay_ms(1);
}

// ── SOFT BLINK (quadratic gamma fade) ────────────────────────

static void soft_blink(void) {
    uint16_t half       = BLINK_ON_MS / 2;
    uint16_t step_delay = half / FADE_STEPS;
    if (step_delay < 1) step_delay = 1;

    for (uint8_t i = 0; i <= FADE_STEPS; i++) {
        set_brightness((uint8_t)((uint32_t)i * i * 255
                       / ((uint32_t)FADE_STEPS * FADE_STEPS)));
        delay_ms(step_delay);
    }
    for (int8_t i = FADE_STEPS; i >= 0; i--) {
        set_brightness((uint8_t)((uint32_t)i * i * 255
                       / ((uint32_t)FADE_STEPS * FADE_STEPS)));
        delay_ms(step_delay);
    }
    set_brightness(0);
}

// ── MARKER BURST ──────────────────────────────────────────────
// 6 rapid hard on/off flashes — no fading, no ambiguity.
// Used identically for both START and END of message.

static void marker_burst(void) {
    delay_ms(BURST_PRE_PAUSE);
    for (uint8_t i = 0; i < BURST_FLASHES; i++) {
        set_brightness(255);
        delay_ms(BURST_ON_MS);
        set_brightness(0);
        if (i < BURST_FLASHES - 1) delay_ms(BURST_OFF_MS);
    }
    delay_ms(BURST_POST_PAUSE);
}

// ── TAP CODE LOGIC ────────────────────────────────────────────

static void blink_n(uint8_t n) {
    for (uint8_t i = 0; i < n; i++) {
        soft_blink();
        if (i < n - 1) delay_ms(BLINK_OFF_MS);
    }
}

static void blink_letter(char c) {
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    if (c < 'A' || c > 'Z') return;
    uint8_t idx = (uint8_t)(c - 'A');
    blink_n(POLYBIUS[idx][0]);
    delay_ms(GROUP_PAUSE_MS);
    blink_n(POLYBIUS[idx][1]);
}

static void blink_word(void) {
    uint8_t len = (uint8_t)strlen(CODE);
    for (uint8_t i = 0; i < len; i++) {
        blink_letter(CODE[i]);
        if (i < len - 1) delay_ms(LETTER_PAUSE_MS);
    }
}

// ── MAIN ──────────────────────────────────────────────────────

int main(void) {
    pwm_init();
    set_brightness(0);
    delay_ms(500);

    uint16_t repeats = 0;
    while (1) {
        if (REPEAT_COUNT > 0 && repeats >= REPEAT_COUNT) {
            set_brightness(0);
            while (1);
        }

        marker_burst();   // ← START marker
        blink_word();
        marker_burst();   // ← END marker

        repeats++;
        delay_ms(WORD_PAUSE_MS);
    }

    return 0;
}
