#pragma once
#include "LiveData.h"
#include "AppConfig.h"
#include <cstddef>

namespace SunSpecMap {
  void build(uint16_t *regs, size_t n, const LiveData &live, const AppConfig &cfg);
}
