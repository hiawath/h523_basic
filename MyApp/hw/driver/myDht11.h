#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define DHT11_PIN   GPIO_PIN_0
#define DHT11_PORT  GPIOB

typedef struct {
  float temperature;
  float humidity;
  bool  is_valid;
} dht11Data_t;

void dht11Init(void);
bool dht11Read(dht11Data_t *data);
float dht11GetTemperature(void);
float dht11GetHumidity(void);
