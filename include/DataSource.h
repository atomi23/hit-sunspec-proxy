#pragma once
#include <Arduino.h>

enum class DataOrigin : uint8_t {
  None = 0,
  Demo = 1,
  Native = 2, // HIT Modbus
  Mqtt = 3,
  Cloud = 4,
};

inline const char *dataOriginLabel(DataOrigin o) {
  switch (o) {
    case DataOrigin::Demo: return "DEMO";
    case DataOrigin::Native: return "NATIVE";
    case DataOrigin::Mqtt: return "MQTT";
    case DataOrigin::Cloud: return "CLOUD";
    default: return "—";
  }
}

namespace DataBroker {
  void begin();
  void reconcile(); // Priorität: Native → MQTT → Cloud
}
