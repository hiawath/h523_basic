#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define HCSR04_TRIG_PIN   GPIO_PIN_8
#define HCSR04_TRIG_PORT  GPIOA

#define HCSR04_ECHO_PIN   GPIO_PIN_10
#define HCSR04_ECHO_PORT  GPIOB

void hcSr04Init(void);
bool hcSr04Read(float *distance_cm);
float hcSr04GetDistance(void);
