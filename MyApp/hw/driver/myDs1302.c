#include "myDs1302.h"
#include <stdio.h>
#include <string.h>

/* DS1302 레지스터 주소 */
#define DS1302_REG_SEC           0x80
#define DS1302_REG_MIN           0x82
#define DS1302_REG_HOUR          0x84
#define DS1302_REG_DATE          0x86
#define DS1302_REG_MONTH         0x88
#define DS1302_REG_DAY           0x8A
#define DS1302_REG_YEAR          0x8C
#define DS1302_REG_WP            0x8E
#define DS1302_REG_TRICKLE       0x90
#define DS1302_REG_BURST_CLOCK   0xBE

static inline uint8_t decToBcd(uint8_t val)
{
  return (uint8_t)(((val / 10) << 4) | (val % 10));
}

static inline uint8_t bcdToDec(uint8_t val)
{
  return (uint8_t)(((val >> 4) * 10) + (val & 0x0F));
}

/* 마이크로초 딜레이 (STM32F4 84MHz 기준 소프트웨어 딜레이 루프) */
static void delayUs(uint32_t us)
{
  volatile uint32_t count = us * 14;
  while (count--)
  {
    __NOP();
  }
}

static void ds1302GpioInit(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* PA0(RST/CE), PA4(CLK): Output Push-Pull */
  GPIO_InitStruct.Pin = DS1302_RST_PIN | DS1302_CLK_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA1(DAT): Output Open-Drain with Pull-up (양방향 입출력 가능) */
  GPIO_InitStruct.Pin = DS1302_DAT_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DS1302_DAT_PORT, &GPIO_InitStruct);

  HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DS1302_DAT_PORT, DS1302_DAT_PIN, GPIO_PIN_SET);
}

static void ds1302WriteByte(uint8_t data)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    HAL_GPIO_WritePin(DS1302_DAT_PORT, DS1302_DAT_PIN, (data & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    delayUs(2);

    HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_SET);
    delayUs(2);
    HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_RESET);
    delayUs(2);

    data >>= 1;
  }
}

static uint8_t ds1302ReadByte(void)
{
  uint8_t data = 0;

  /* 오픈드레인 핀을 HIGH 상태(입력 대기)로 설정 */
  HAL_GPIO_WritePin(DS1302_DAT_PORT, DS1302_DAT_PIN, GPIO_PIN_SET);

  for (uint8_t i = 0; i < 8; i++)
  {
    if (HAL_GPIO_ReadPin(DS1302_DAT_PORT, DS1302_DAT_PIN) == GPIO_PIN_SET)
    {
      data |= (1 << i);
    }
    HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_SET);
    delayUs(2);
    HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_RESET);
    delayUs(2);
  }

  return data;
}

static void ds1302WriteReg(uint8_t reg, uint8_t value)
{
  HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_SET);
  delayUs(4);

  ds1302WriteByte(reg & 0xFE); /* Write Command (Bit 0 = 0) */
  ds1302WriteByte(value);

  HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_RESET);
  delayUs(4);
}

static uint8_t ds1302ReadReg(uint8_t reg)
{
  uint8_t val = 0;

  HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_SET);
  delayUs(4);

  ds1302WriteByte(reg | 0x01); /* Read Command (Bit 0 = 1) */
  val = ds1302ReadByte();

  HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_RESET);
  delayUs(4);

  return val;
}

/* 요일 자동 계산 함수 (1: Sun, 2: Mon, ... 7: Sat) */
static uint8_t calculateDayOfWeek(uint16_t y, uint8_t m, uint8_t d)
{
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3)
    y -= 1;
  int dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
  return (uint8_t)(dow + 1); // 1 = Sun, ..., 7 = Sat
}

const char* ds1302GetDayStr(uint8_t day_of_week)
{
  static const char *days[] = {"???", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  if (day_of_week >= 1 && day_of_week <= 7)
    return days[day_of_week];
  return "???";
}

void ds1302SetDateTime(const ds1302Time_t *time)
{
  if (!time)
    return;

  ds1302WriteReg(DS1302_REG_WP, 0x00); // Write Protect 해제

  ds1302WriteReg(DS1302_REG_SEC, decToBcd(time->sec) & 0x7F); // CH=0 (오실레이터 동작)
  ds1302WriteReg(DS1302_REG_MIN, decToBcd(time->min) & 0x7F);
  ds1302WriteReg(DS1302_REG_HOUR, decToBcd(time->hour) & 0x3F); // 24시간 모드
  ds1302WriteReg(DS1302_REG_DATE, decToBcd(time->day) & 0x3F);
  ds1302WriteReg(DS1302_REG_MONTH, decToBcd(time->month) & 0x1F);
  ds1302WriteReg(DS1302_REG_DAY, decToBcd(time->day_of_week) & 0x07);
  ds1302WriteReg(DS1302_REG_YEAR, decToBcd((uint8_t)(time->year % 100)));

  ds1302WriteReg(DS1302_REG_WP, 0x80); // Write Protect 활성화
}

void ds1302SetTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
  ds1302Time_t t;
  t.year = year;
  t.month = month;
  t.day = day;
  t.day_of_week = calculateDayOfWeek(year, month, day);
  t.hour = hour;
  t.min = min;
  t.sec = sec;

  ds1302SetDateTime(&t);
}

bool ds1302GetDateTime(ds1302Time_t *time)
{
  if (!time)
    return false;

  uint8_t sec_raw  = ds1302ReadReg(DS1302_REG_SEC);
  uint8_t min_raw  = ds1302ReadReg(DS1302_REG_MIN);
  uint8_t hour_raw = ds1302ReadReg(DS1302_REG_HOUR);
  uint8_t date_raw = ds1302ReadReg(DS1302_REG_DATE);
  uint8_t mon_raw  = ds1302ReadReg(DS1302_REG_MONTH);
  uint8_t day_raw  = ds1302ReadReg(DS1302_REG_DAY);
  uint8_t year_raw = ds1302ReadReg(DS1302_REG_YEAR);

  time->sec         = bcdToDec(sec_raw & 0x7F);
  time->min         = bcdToDec(min_raw & 0x7F);
  time->hour        = bcdToDec(hour_raw & 0x3F);
  time->day         = bcdToDec(date_raw & 0x3F);
  time->month       = bcdToDec(mon_raw & 0x1F);
  time->day_of_week = bcdToDec(day_raw & 0x07);
  time->year        = 2000 + bcdToDec(year_raw);

  return true;
}

/**
  * @brief  빌드 날짜와 시간(__DATE__, __TIME__)으로 DS1302 시간 설정
  */
void ds1302SetBuildTime(void)
{
  //const char *date_str = __DATE__; // "Mmm dd yyyy" e.g. "Aug 15 2026"
  const char *time_str = __TIME__; // "hh:mm:ss"    e.g. "12:40:00"

  //char month_str[4] = {0};
  //int day = 0, year = 0;
  int hour = 0, min = 0, sec = 0;

  const char *date_str = __DATE__; // "Mmm dd yyyy" 형식 고정 (11글자)
  char month_str[4] = { date_str[0], date_str[1], date_str[2], '\0' };
  
  // day 파싱 (' ' 공백 처리 포함)
  int day = (date_str[4] == ' ' ? 0 : (date_str[4] - '0') * 10) + (date_str[5] - '0');
  
  // year 파싱
  int year = (date_str[7] - '0') * 1000 + (date_str[8] - '0') * 100 + 
              (date_str[9] - '0') * 10 + (date_str[10] - '0');


  sscanf(date_str, "%3s %d %d", month_str, &day, &year);
  sscanf(time_str, "%d:%d:%d", &hour, &min, &sec);

  static const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  uint8_t month = 1;
  for (uint8_t i = 0; i < 12; i++)
  {
    if (strncmp(month_str, months[i], 3) == 0)
    {
      month = i + 1;
      break;
    }
  }

  ds1302SetTime((uint16_t)year, month, (uint8_t)day, (uint8_t)hour, (uint8_t)min, (uint8_t)sec);
}

void ds1302Init(void)
{
  ds1302GpioInit();

  /* Write Protect 해제 */
  ds1302WriteReg(DS1302_REG_WP, 0x00);

  /* Clock Halt(CH) 비트 확인 (SEC 레지스터 bit7)
   * CH=1: 오실레이터 정지 상태 (배터리 방전 또는 최초 기동) → 빌드 시간으로 초기화
   * CH=0: 오실레이터 정상 동작 중 → 기존 시간 유지 */
  uint8_t sec = ds1302ReadReg(DS1302_REG_SEC);
  if (sec & 0x80)
  {
    ds1302SetBuildTime();
  }
  //ds1302SetBuildTime();
  /* else: RTC가 이미 정상 동작 중이므로 시간을 덮어쓰지 않음 */
}
