#include "clock.h"
#include "pico/stdlib.h"
#include "hardware/rtc.h"
#include <stdio.h>
#include <stdbool.h>
#include <time.h>


#define TZ_OFFSET_HOURS (-5)


static volatile bool g_time_valid = false;


void clock_init(void) {
    rtc_init();         
}

bool clock_time_valid(void) {
    return g_time_valid;
}

bool clock_set_epoch_utc(time_t epoch_utc) {
    struct tm *ti = gmtime(&epoch_utc);
    if (!ti) return false;

    datetime_t t = {
        .year  = ti->tm_year + 1900,
        .month = ti->tm_mon + 1,
        .day   = ti->tm_mday,
        .dotw  = ti->tm_wday,
        .hour  = ti->tm_hour,
        .min   = ti->tm_min,
        .sec   = ti->tm_sec
    };

    rtc_init();
    if (!rtc_set_datetime(&t)) return false;

    g_time_valid = true;
    return true;
}


static void apply_timezone_simple(datetime_t *t, int offset_hours) {
    int h = (int)t->hour + offset_hours;

    if (h < 0) {
        h += 24;
        t->dotw = (t->dotw + 6) % 7;   // previous day
        if (t->day > 1) t->day -= 1;   
    } else if (h >= 24) {
        h -= 24;
        t->dotw = (t->dotw + 1) % 7;   // next day
        t->day += 1;                   
    }

    t->hour = (int8_t)h;
}


bool clock_get_local_datetime(datetime_t *out) {
    if (!g_time_valid) return false;

    datetime_t t;
    if (!rtc_get_datetime(&t)) return false;

    apply_timezone_simple(&t, TZ_OFFSET_HOURS);
    *out = t;
    return true;
}


void usb_time_sync_poll(void) {
    static char buf[64];
    static size_t n = 0;

    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\r') continue;

        if (c == '\n') {
            buf[n] = 0;
            n = 0;

            long long epoch = 0;
            if (sscanf(buf, "T %lld", &epoch) != 1) {
                printf("ERR fmt\n");
                continue;
            }

            if (epoch <= 1700000000LL || epoch >= 4102444800LL) {
                printf("ERR range\n");
                continue;
            }

            if (clock_set_epoch_utc((time_t)epoch)) printf("OK\n");
            else printf("ERR rtc\n");

            continue;
        }

        if (n + 1 < sizeof(buf)) {
            buf[n++] = (char)c;
        } else {
            n = 0;
            printf("ERR overflow\n");
        }
    }
}

