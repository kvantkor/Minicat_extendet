#include "rtc.h"
#include "kernel.h" // для outb/inb

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t read_register(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

// Конвертация BCD в обычное число
static int bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void rtc_get_time(int *h, int *m, int *s) {
    // Ждем, пока часы не будут в состоянии обновления (базово)
    *s = bcd_to_bin(read_register(0x00));
    *m = bcd_to_bin(read_register(0x02));
    *h = bcd_to_bin(read_register(0x04));
}
