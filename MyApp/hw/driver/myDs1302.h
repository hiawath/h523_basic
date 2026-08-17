#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define DS1302_RST_PIN    GPIO_PIN_0
#define DS1302_RST_PORT   GPIOA

#define DS1302_DAT_PIN    GPIO_PIN_1
#define DS1302_DAT_PORT   GPIOA

#define DS1302_CLK_PIN    GPIO_PIN_4
#define DS1302_CLK_PORT   GPIOA

typedef struct {
  uint16_t year;        /* 2000 ~ 2099 */
  uint8_t  month;       /* 1 ~ 12 */
  uint8_t  day;         /* 1 ~ 31 */
  uint8_t  day_of_week; /* 1 ~ 7 (1: Sun, 2: Mon, ... 7: Sat) */
  uint8_t  hour;        /* 0 ~ 23 */
  uint8_t  min;         /* 0 ~ 59 */
  uint8_t  sec;         /* 0 ~ 59 */
} ds1302Time_t;

void ds1302Init(void);
void ds1302SetTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);
void ds1302SetDateTime(const ds1302Time_t *time);
bool ds1302GetDateTime(ds1302Time_t *time);
void ds1302SetBuildTime(void);
const char* ds1302GetDayStr(uint8_t day_of_week);
