#include "apMain.h"
#include "myHw.h"
#include <stdio.h>

/**
  * @brief  Application Main Entry Point (메인 루프)
  */
void apMain(void)
{
  /* SSD1306 초기 화면 프레임 및 타이틀 출력 */
  ssd1306Clear();
  ssd1306DrawRect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_COLOR_WHITE);
  ssd1306DrawString(8, 3, "STM32 MULTI-SENSOR", SSD1306_COLOR_WHITE);
  ssd1306DrawLine(4, 13, 124, 13, SSD1306_COLOR_WHITE);
  ssd1306Update();

  /* DMA 방식 ADC 내부 온도 측정 시작 (샘플링 주기: 500ms) */
  adcStartDMA(500);

  /* LCD1602 초기 화면 출력 */
  lcd1602Clear();
  lcd1602Cursor(0, 0);
  lcd1602Print("HC-SR04 Distance");
  lcd1602Cursor(1, 0);
  lcd1602Print("Dist:    0.0 cm ");

  uint32_t prev_200ms_tick = HAL_GetTick();
  uint32_t prev_1s_tick    = HAL_GetTick();
  uint32_t prev_2s_tick    = HAL_GetTick();
  uint32_t uptime_sec      = 0;
  char str_buf[32];

  ds1302Time_t rtc_time = {0};
  dht11Data_t dht_data  = {0};
  float distance_cm     = 0.0f;

  /* 첫 1회 DHT11 즉시 읽기 */
  dht11Read(&hdht11, &dht_data);

  while (1)
  {
    /* ADC 샘플링 주기 관리 및 DMA 완료 시 내부 온도 계산 */
    adcUpdate();

    /* 1. 2000ms(2초)마다 DHT11 온습도 센서 읽기 (DHT11 권장 샘플링 주기 준수) */
    if (HAL_GetTick() - prev_2s_tick >= 2000)
    {
      prev_2s_tick = HAL_GetTick();
      dht11Read(&hdht11, &dht_data);
    }

    /* 2. 200ms마다 HC-SR04 초음파 거리 측정 및 LCD1602 화면 갱신 */
    if (HAL_GetTick() - prev_200ms_tick >= 200)
    {
      prev_200ms_tick = HAL_GetTick();

      /* 거리 측정 수행 */
      if (hcSr04Read(&distance_cm))
      {
        lcd1602Cursor(1, 0);
        lcd1602Printf("Dist: %6.1f cm ", distance_cm);
      }
      else
      {
        lcd1602Cursor(1, 0);
        lcd1602Printf("Dist:  ---.- cm ");
      }
    }

    /* 3. 1000ms(1초)마다 RTC 시계 읽기 및 SSD1306/UART 실시간 갱신 */
    if (HAL_GetTick() - prev_1s_tick >= 1000)
    {
      prev_1s_tick = HAL_GetTick();
      uptime_sec++;

      /* DS1302 RTC 읽기 (핸들 기반) */
      ds1302GetDateTime(&hds1302, &rtc_time);

      /* 내부 온도 취득 */
      float int_temp = adcGetTemp();

      /* SSD1306 날짜 표시 (y=17) */
      ssd1306FillRect(6, 17, 116, 9, SSD1306_COLOR_BLACK);
      snprintf(str_buf, sizeof(str_buf), "%04d-%02d-%02d (%s)", 
               rtc_time.year, rtc_time.month, rtc_time.day, ds1302GetDayStr(rtc_time.day_of_week));
      ssd1306DrawString(6, 17, str_buf, SSD1306_COLOR_WHITE);

      /* SSD1306 시간 표시 (y=28) */
      ssd1306FillRect(6, 28, 116, 9, SSD1306_COLOR_BLACK);
      snprintf(str_buf, sizeof(str_buf), "Time: %02d:%02d:%02d", 
               rtc_time.hour, rtc_time.min, rtc_time.sec);
      ssd1306DrawString(6, 28, str_buf, SSD1306_COLOR_WHITE);

      /* SSD1306 온도 표시: 내부 / DHT11 (y=39) */
      ssd1306FillRect(6, 39, 116, 9, SSD1306_COLOR_BLACK);
      snprintf(str_buf, sizeof(str_buf), "T: %.1f / %.1f C", int_temp, dht_data.temperature);
      ssd1306DrawString(6, 39, str_buf, SSD1306_COLOR_WHITE);

      /* SSD1306 습도 표시 (y=51) */
      ssd1306FillRect(6, 51, 116, 9, SSD1306_COLOR_BLACK);
      snprintf(str_buf, sizeof(str_buf), "Humidity: %.1f %%", dht_data.humidity);
      ssd1306DrawString(6, 51, str_buf, SSD1306_COLOR_WHITE);

      ssd1306Update();

      /* UART 로그 출력 */
      printf("[DATA] %04d-%02d-%02d (%s) %02d:%02d:%02d | Int: %.1f C | DHT11: %.1f C, %.1f %% | Dist: %.1f cm | Up: %lus\r\n",
             rtc_time.year, rtc_time.month, rtc_time.day, ds1302GetDayStr(rtc_time.day_of_week),
             rtc_time.hour, rtc_time.min, rtc_time.sec,
             int_temp, dht_data.temperature, dht_data.humidity, distance_cm, (unsigned long)uptime_sec);
    }

    HAL_Delay(2);
  }
}
