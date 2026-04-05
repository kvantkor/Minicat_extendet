#ifndef RTC_H
#define RTC_H

#include <stdint.h>

void rtc_get_time(int *h, int *m, int *s);
void rtc_get_date(int *d, int *mo, int *y);

#endif

