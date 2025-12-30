#ifndef CLOCK_H
#define CLOCK_H

#include <stdbool.h>
#include <time.h>
#include "hardware/rtc.h"


void clock_init(void);
void usb_time_sync_poll(void);

bool clock_time_valid(void);
bool clock_set_epoch_utc(time_t epoch_utc);

bool clock_get_local_datetime(datetime_t *out);


#endif
