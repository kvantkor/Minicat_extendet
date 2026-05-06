#ifndef RTC_H
#define RTC_H

#include <stdint.h>

/**
 * Читает текущее время из часов реального времени (RTC).
 * @param h - указатель для записи часов
 * @param m - указатель для записи минут
 * @param s - указатель для записи секунд
 */
void rtc_get_time(int *h, int *m, int *s);

/**
 * Читает текущую дату из RTC.
 * @param d  - указатель для записи дня
 * @param mo - указатель для записи месяца
 * @param y  - указатель для записи года
 */
void rtc_get_date(int *d, int *mo, int *y);

#endif
