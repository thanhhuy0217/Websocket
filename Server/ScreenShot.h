#pragma once

#include <vector>
#include <cstdint>

// Chụp toàn bộ màn hình chính, trả về dữ liệu pixel BGRA 32-bit:
// size = width * height * 4 bytes.
//
// width, height = GetSystemMetrics(SM_CXSCREEN / SM_CYSCREEN).
// Nếu lỗi -> trả về vector rỗng.
std::vector<std::uint8_t> ScreenShot();
