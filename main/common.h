/******************************************************************************
 * Copyright (c) 2025 Marconatale Parise.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************/
/**
 * @file common.h
 * @brief this file contain common macros and definitions used across the project.
 *
 * 
 * @author Marconatale Parise
 * @date 19 Feb 2026
 *
 */
#pragma once

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"


#ifndef __COMMON_H__
#define __COMMON_H__


#define DEBUG 1
#define DEBUG_BT 0
#define DEBUG_ADC 0
#define DEBUG_GPIO 0
#define DEBUG_DAC 0

#if DEBUG
#define LOG(x,...) if(DEBUG){ ESP_LOGI("APP", x, ##__VA_ARGS__);}
#define LOG_ADC(x,...) if(DEBUG_ADC){ ESP_LOGI("ADC_HAL", x, ##__VA_ARGS__);}
#define LOG_GPIO(x,...) if(DEBUG_GPIO){ ESP_LOGI("GPIO_HAL", x, ##__VA_ARGS__);}
#define LOG_BT(x,...) if(DEBUG_BT){ ESP_LOGI("BT_HAL", x, ##__VA_ARGS__);}
#define LOG_DAC(x,...) if(DEBUG_DAC){ ESP_LOGI("DAC_HAL", x, ##__VA_ARGS__);}

#endif


#endif