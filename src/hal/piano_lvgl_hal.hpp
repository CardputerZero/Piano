#pragma once

#include <cstdint>

namespace piano {

bool initLvglHal(int32_t width, int32_t height);
void shutdownLvglHal();

}  // namespace piano
