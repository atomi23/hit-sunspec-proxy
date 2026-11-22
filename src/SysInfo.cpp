#include "SysInfo.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr uint32_t SAMPLE_MS = 2000;
constexpr int CORES = portNUM_PROCESSORS;

uint32_t lastMs = 0;
uint32_t lastUs = 0;
uint32_t lastIdle[CORES] = {};
int8_t load[CORES];
bool haveLoad = false;

// Laufzeit des Leerlauf-Tasks je Kern in Mikrosekunden (FreeRTOS-Laufzeitstatistik)
uint32_t idleRunTime(int core) {
  TaskStatus_t st = {};
  vTaskGetInfo(xTaskGetIdleTaskHandleForCore(core), &st, pdFALSE, eRunning);
  return uint32_t(st.ulRunTimeCounter);
}
} // namespace

namespace SysInfo {
// Last = Anteil der Zeit, in der der Kern nicht im Leerlauf-Task war, gemittelt über SAMPLE_MS.
void loop() {
  const uint32_t now = millis();
  if (lastMs && now - lastMs < SAMPLE_MS) return;
  const uint32_t us = micros();
  const uint32_t dt = us - lastUs;
  for (int i = 0; i < CORES; i++) {
    const uint32_t idle = idleRunTime(i);
    if (lastMs && dt) {
      const uint32_t d = idle - lastIdle[i];
      load[i] = int8_t(d >= dt ? 0 : 100 - uint64_t(d) * 100 / dt);
    }
    lastIdle[i] = idle;
  }
  haveLoad = lastMs != 0;
  lastMs = now ? now : 1;
  lastUs = us;
}

void fill(JsonObject out) {
  out["chip"] = ESP.getChipModel();
  out["chipRev"] = ESP.getChipRevision();
  out["cores"] = ESP.getChipCores();
  out["cpuMhz"] = ESP.getCpuFreqMHz();
  out["flashBytes"] = ESP.getFlashChipSize();
  out["psramBytes"] = ESP.getPsramSize();
  out["psramFree"] = ESP.getFreePsram();
  out["heapBytes"] = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  out["heapFree"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  out["heapMin"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  out["heapBlock"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  out["sketchBytes"] = ESP.getSketchSize();
  out["sketchFree"] = ESP.getFreeSketchSpace(); // Größe der OTA-Partition, nicht der Rest
  out["fsBytes"] = LittleFS.totalBytes();
  out["fsUsed"] = LittleFS.usedBytes();
  out["sdk"] = ESP.getSdkVersion();
  out["arduino"] = ESP_ARDUINO_VERSION_STR;
  out["build"] = __DATE__ " " __TIME__;
  out["tasks"] = uxTaskGetNumberOfTasks();
  JsonArray a = out["cpu"].to<JsonArray>();
  for (int i = 0; haveLoad && i < CORES; i++) a.add(load[i]);
}
} // namespace SysInfo
