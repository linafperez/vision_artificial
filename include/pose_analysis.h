#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace vision {

struct PoseAnalysisOptions {
    std::string input = "camera";
    std::filesystem::path model_path = "models/pose_landmarker.task";
    std::optional<std::filesystem::path> calibration_path;
    std::optional<std::filesystem::path> output_video_path;
    bool display = true;
};

int runPoseAnalysis(const PoseAnalysisOptions& options);

}  // namespace vision
