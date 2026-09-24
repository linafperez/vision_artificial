#pragma once

#include <filesystem>

namespace vision {

struct CalibrationOptions {
    std::filesystem::path image_directory;
    std::filesystem::path output_directory;
    int board_columns = 8;
    int board_rows = 5;
    float square_size_mm = 26.0F;
};

int runCalibration(const CalibrationOptions& options);

}  // namespace vision
