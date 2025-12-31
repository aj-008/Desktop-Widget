/**************************************************************
 *
 *                          clock.c
 *
 *     Author:  AJ Romeo
 *
 *     Implementation of real-time clock with USB serial time
 *     synchronization. Manages hardware RTC and provides
 *     timezone-aware time access.
 *
 **************************************************************/

#include "clock.h"
#include "pico/stdlib.h"
#include "hardware/rtc.h"
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#define TZ_OFFSET_HOURS (-5)

static volatile bool g_time_valid = false;

static void apply_timezone_offset(datetime_t *t, int offset_hours);

/********** clock_init ********
 *
 * Initialize the hardware RTC module
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      Called once at system startup
 *
 * Notes:
 *      Must be called before any other clock functions
 ************************/
void clock_init(void)
{
        rtc_init();
}

/********** clock_time_valid ********
 *
 * Check if clock has been set to a valid time
 *
 * Parameters:
 *      none
 *
 * Return: true if clock has been synchronized, false otherwise
 *
 * Expects:
 *      none
 *
 * Notes:
 *      Clock is valid after successful call to clock_set_epoch_utc
 ************************/
bool clock_time_valid(void)
{
        return g_time_valid;
}

/********** clock_set_epoch_utc ********
 *
 * Set the RTC using Unix epoch timestamp in UTC
 *
 * Parameters:
 *      time_t epoch_utc: Unix timestamp (seconds since 1970-01-01)
 *
 * Return: true if successful, false on error
 *
 * Expects:
 *      epoch_utc to be a valid Unix timestamp
 *
 * Notes:
 *      Converts epoch to datetime structure and programs RTC
 *      Marks clock as valid on success
 ************************/
bool clock_set_epoch_utc(time_t epoch_utc)
{
        struct tm *ti = gmtime(&epoch_utc);
        if (ti == NULL) {
                return false;
        }

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
        if (rtc_set_datetime(&t) == false) {
                return false;
        }

        g_time_valid = true;
        return true;
}

/********** apply_timezone_offset ********
 *
 * Apply hour-based timezone offset to datetime
 *
 * Parameters:
 *      datetime_t *t:      datetime to modify in-place
 *      int offset_hours:   timezone offset in hours (e.g., -5 for EST)
 *
 * Return: none
 *
 * Expects:
 *      t is not NULL
 *
 * Notes:
 *      Handles day wraparound
 *      Does not account for month/year boundaries or DST
 *      Sufficient for display purposes
 ************************/
static void apply_timezone_offset(datetime_t *t, int offset_hours)
{
        int h = (int)t->hour + offset_hours;

        if (h < 0) {
                h += 24;
                t->dotw = (t->dotw + 6) % 7;
                if (t->day > 1) {
                        t->day -= 1;
                }
        } else if (h >= 24) {
                h -= 24;
                t->dotw = (t->dotw + 1) % 7;
                t->day += 1;
        }

        t->hour = (int8_t)h;
}

/********** clock_get_local_datetime ********
 *
 * Get current time from RTC with timezone offset applied
 *
 * Parameters:
 *      datetime_t *out: pointer to datetime structure to fill
 *
 * Return: true if successful, false if clock not valid or RTC error
 *
 * Expects:
 *      out is not NULL
 *
 * Notes:
 *      Applies TZ_OFFSET_HOURS to UTC time from RTC
 ************************/
bool clock_get_local_datetime(datetime_t *out)
{
        if (g_time_valid == false) {
                return false;
        }

        datetime_t t;
        if (rtc_get_datetime(&t) == false) {
                return false;
        }

        apply_timezone_offset(&t, TZ_OFFSET_HOURS);
        *out = t;
        return true;
}

/********** usb_time_sync_poll ********
 *
 * Poll USB serial for time synchronization commands
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      USB serial initialized with stdio_init_all()
 *
 * Notes:
 *      Expected format: "T <epoch>\n" where epoch is Unix timestamp
 *      Responses: "OK", "ERR fmt", "ERR range", "ERR rtc", 
 *                 "ERR overflow"
 *      Should be called regularly from main loop
 *      Validates timestamp range (2023-2100)
 ************************/
void usb_time_sync_poll(void)
{
        static char buf[64];
        static size_t n = 0;
        int c;

        while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
                if (c == '\r') {
                        continue;
                }

                if (c == '\n') {
                        buf[n] = '\0';
                        n = 0;

                        long long epoch = 0;
                        if (sscanf(buf, "T %lld", &epoch) != 1) {
                                printf("ERR fmt\n");
                                continue;
                        }

                        if (epoch <= 1700000000LL ||
                            epoch >= 4102444800LL) {
                                printf("ERR range\n");
                                continue;
                        }

                        if (clock_set_epoch_utc((time_t)epoch)) {
                                printf("OK\n");
                        } else {
                                printf("ERR rtc\n");
                        }
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
