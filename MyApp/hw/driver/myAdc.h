#pragma once

#include "main.h"
#include "adc.h"
#include <stdint.h>
#include <stdbool.h>

void adcInit(void);
void adcStartDMA(uint32_t interval_ms);
void adcSetInterval(uint32_t interval_ms);
void adcUpdate(void);

uint32_t adcGetRaw(void);
float adcGetTemp(void);
bool adcIsUpdated(void);
