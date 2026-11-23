#pragma once
#include <cstdint>

using TickType_t = uint32_t;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
constexpr int pdTRUE = 1;
inline TickType_t pdMS_TO_TICKS(uint32_t ms) { return ms; }

using TaskHandle_t = void *;
uint32_t ulTaskNotifyTake(int, TickType_t);
int xTaskCreatePinnedToCore(void (*run)(void *), const char *, uint32_t, void *, int, TaskHandle_t *, int);
inline void xTaskNotifyGive(TaskHandle_t) {}
inline void vTaskDelay(TickType_t) {}
