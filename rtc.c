#include "rtc.h"
#include "kernel.h" // для outb/inb

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static int is_updating() {
    outb(CMOS_ADDR, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

static uint8_t read_register(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

// Конвертация BCD в обычное число
static int bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}


void rtc_get_time(int *h, int *m, int *s) {
    // Ждем окончания обновления, чтобы не прочитать "переключающиеся" цифры
    while (is_updating());

    uint8_t sec  = read_register(0x00);
    uint8_t min  = read_register(0x02);
    uint8_t hour = read_register(0x04);
    uint8_t registerB = read_register(0x0B);

    // Если бит 2 регистра B равен 0, значит данные в BCD формате
    if (!(registerB & 0x04)) {
        *s = bcd_to_bin(sec);
        *m = bcd_to_bin(min);
        *h = bcd_to_bin(hour);
    } else {
        *s = sec;
        *m = min;
        *h = hour;
    }

    // Обработка 12-часового формата (если бит 1 регистра B равен 0)
    if (!(registerB & 0x02) && (*h & 0x80)) {
        *h = ((*h & 0x7F) + 12) % 24;
    }
}
