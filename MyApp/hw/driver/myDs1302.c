#include "myDs1302.h"
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

/* GPIO 제어 매크로 (핸들 기반) */
#define RST_HIGH(h)  HAL_GPIO_WritePin((h)->pins.rst_port, (h)->pins.rst_pin, GPIO_PIN_SET)
#define RST_LOW(h)   HAL_GPIO_WritePin((h)->pins.rst_port, (h)->pins.rst_pin, GPIO_PIN_RESET)
#define CLK_HIGH(h)  HAL_GPIO_WritePin((h)->pins.clk_port, (h)->pins.clk_pin, GPIO_PIN_SET)
#define CLK_LOW(h)   HAL_GPIO_WritePin((h)->pins.clk_port, (h)->pins.clk_pin, GPIO_PIN_RESET)
#define DAT_HIGH(h)  HAL_GPIO_WritePin((h)->pins.dat_port, (h)->pins.dat_pin, GPIO_PIN_SET)
#define DAT_LOW(h)   HAL_GPIO_WritePin((h)->pins.dat_port, (h)->pins.dat_pin, GPIO_PIN_RESET)
#define DAT_READ(h)  HAL_GPIO_ReadPin((h)->pins.dat_port, (h)->pins.dat_pin)

static inline uint8_t decToBcd(uint8_t val)
{
  return (uint8_t)(((val / 10) << 4) | (val % 10));
}

static inline uint8_t bcdToDec(uint8_t val)
{
  return (uint8_t)(((val >> 4) * 10) + (val & 0x0F));
}

/* 마이크로초 딜레이 (STM32H523 250MHz 기준 소프트웨어 루프) */
static void delayUs(uint32_t us)
{
  volatile uint32_t count = us * 42;
  while (count--)
  {
    __NOP();
  }
}

static void ds1302GpioInit(ds1302Handle_t *hds)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* RST, CLK: Output Push-Pull */
  if (hds->pins.rst_port == hds->pins.clk_port)
  {
    GPIO_InitStruct.Pin   = hds->pins.rst_pin | hds->pins.clk_pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(hds->pins.rst_port, &GPIO_InitStruct);
  }
  else
  {
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Pin = hds->pins.rst_pin;
    HAL_GPIO_Init(hds->pins.rst_port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = hds->pins.clk_pin;
    HAL_GPIO_Init(hds->pins.clk_port, &GPIO_InitStruct);
  }

  /* DAT: Output Open-Drain with Pull-up (양방향 입출력) */
  GPIO_InitStruct.Pin   = hds->pins.dat_pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull  = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(hds->pins.dat_port, &GPIO_InitStruct);

  /* 초기 핀 상태 */
  RST_LOW(hds);
  CLK_LOW(hds);
  DAT_HIGH(hds);
}

static void ds1302WriteByte(ds1302Handle_t *hds, uint8_t data)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (data & 0x01)
      DAT_HIGH(hds);
    else
      DAT_LOW(hds);

    delayUs(2);

    CLK_HIGH(hds);
    delayUs(2);
    CLK_LOW(hds);
    delayUs(2);

    data >>= 1;
  }
}

static uint8_t ds1302ReadByte(ds1302Handle_t *hds)
{
  uint8_t data = 0;

  DAT_HIGH(hds);

  for (uint8_t i = 0; i < 8; i++)
  {
    if (DAT_READ(hds) == GPIO_PIN_SET)
    {
      data |= (1 << i);
    }
    CLK_HIGH(hds);
    delayUs(2);
    CLK_LOW(hds);
    delayUs(2);
  }

  return data;
}

static void ds1302WriteReg(ds1302Handle_t *hds, uint8_t reg, uint8_t value)
{
  RST_LOW(hds);
  CLK_LOW(hds);
  delayUs(4);

  RST_HIGH(hds);
  delayUs(4);

  ds1302WriteByte(hds, reg & 0xFE); /* Write Command (Bit 0 = 0) */
  ds1302WriteByte(hds, value);

  delayUs(2);
  RST_LOW(hds);
  delayUs(4);
}

static uint8_t ds1302ReadReg(ds1302Handle_t *hds, uint8_t reg)
{
  uint8_t val = 0;

  RST_LOW(hds);
  CLK_LOW(hds);
  delayUs(4);

  RST_HIGH(hds);
  delayUs(4);

  ds1302WriteByte(hds, reg | 0x01); /* Read Command (Bit 0 = 1) */
  val = ds1302ReadByte(hds);

  delayUs(2);
  RST_LOW(hds);
  delayUs(4);

  return val;
}

static const char* const day_names[] = {
  "ERR", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

const char* ds1302GetDayStr(uint8_t day_of_week)
{
  if (day_of_week >= 1 && day_of_week <= 7)
  {
    return day_names[day_of_week];
  }
  return day_names[0];
}

void ds1302SetDateTime(ds1302Handle_t *hds, const ds1302Time_t *time)
{
  if (!hds || !time)
    return;

  ds1302WriteReg(hds, DS1302_REG_WP, 0x00); /* Write Protect 해제 */

  ds1302WriteReg(hds, DS1302_REG_SEC,   decToBcd(time->sec)  & 0x7F); /* CH=0 */
  ds1302WriteReg(hds, DS1302_REG_MIN,   decToBcd(time->min)  & 0x7F);
  ds1302WriteReg(hds, DS1302_REG_HOUR,  decToBcd(time->hour) & 0x3F); /* 24시간 모드 */
  ds1302WriteReg(hds, DS1302_REG_DATE,  decToBcd(time->day)  & 0x3F);
  ds1302WriteReg(hds, DS1302_REG_MONTH, decToBcd(time->month)       & 0x1F);
  ds1302WriteReg(hds, DS1302_REG_DAY,   decToBcd(time->day_of_week) & 0x07);
  ds1302WriteReg(hds, DS1302_REG_YEAR,  decToBcd((uint8_t)(time->year % 100)));

  ds1302WriteReg(hds, DS1302_REG_WP, 0x80); /* Write Protect 활성화 */
}

void ds1302SetTime(ds1302Handle_t *hds, uint16_t year, uint8_t month, uint8_t day,
                   uint8_t hour, uint8_t min, uint8_t sec)
{
  ds1302Time_t t;
  t.year        = year;
  t.month       = month;
  t.day         = day;
  t.day_of_week = 1;
  t.hour        = hour;
  t.min         = min;
  t.sec         = sec;

  ds1302SetDateTime(hds, &t);
}

bool ds1302GetDateTime(ds1302Handle_t *hds, ds1302Time_t *time)
{
  if (!hds || !time)
    return false;

  uint8_t sec_raw  = ds1302ReadReg(hds, DS1302_REG_SEC);
  uint8_t min_raw  = ds1302ReadReg(hds, DS1302_REG_MIN);
  uint8_t hour_raw = ds1302ReadReg(hds, DS1302_REG_HOUR);
  uint8_t date_raw = ds1302ReadReg(hds, DS1302_REG_DATE);
  uint8_t mon_raw  = ds1302ReadReg(hds, DS1302_REG_MONTH);
  uint8_t day_raw  = ds1302ReadReg(hds, DS1302_REG_DAY);
  uint8_t year_raw = ds1302ReadReg(hds, DS1302_REG_YEAR);

  if (sec_raw & 0x80)
  {
    return false;
  }

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
  *         sscanf 미사용 — 스택 절약을 위해 수동 파싱 사용
  */
void ds1302SetBuildTime(ds1302Handle_t *hds)
{
  const char *time_str = __TIME__; /* "hh:mm:ss" */
  const char *date_str = __DATE__; /* "Mmm dd yyyy" */

  /* month 문자열 파싱 (앞 3글자) */
  char month_str[4] = { date_str[0], date_str[1], date_str[2], '\0' };

  /* day 파싱 (' ' 공백 처리 포함: " 5" vs "15") */
  int day  = (date_str[4] == ' ' ? 0 : (date_str[4] - '0') * 10) + (date_str[5] - '0');

  /* year 파싱 */
  int year = (date_str[7] - '0') * 1000 + (date_str[8] - '0') * 100 +
             (date_str[9] - '0') * 10   + (date_str[10] - '0');

  /* time 파싱 "hh:mm:ss" */
  int hour = (time_str[0] - '0') * 10 + (time_str[1] - '0');
  int min  = (time_str[3] - '0') * 10 + (time_str[4] - '0');
  int sec  = (time_str[6] - '0') * 10 + (time_str[7] - '0');

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

  ds1302SetTime(hds, (uint16_t)year, month, (uint8_t)day,
                (uint8_t)hour, (uint8_t)min, (uint8_t)sec);
}

void ds1302Init(ds1302Handle_t *hds, const ds1302Pin_t *pins)
{
  if (!hds || !pins)
    return;

  hds->pins        = *pins;
  hds->initialized = false;

  ds1302GpioInit(hds);
  hds->initialized = true;

  /* Write Protect 해제 */
  ds1302WriteReg(hds, DS1302_REG_WP, 0x00);

  /* Clock Halt(CH) 확인 */
  uint8_t sec = ds1302ReadReg(hds, DS1302_REG_SEC);
  if (sec & 0x80)
  {
    ds1302SetBuildTime(hds);
  }
}
