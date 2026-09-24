#include "pose_analysis.h"

#include "squat_counter.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "mediapipe/framework/formats/image.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/tasks/cc/core/base_options.h"
#include "mediapipe/tasks/cc/vision/core/running_mode.h"
#include "mediapipe/tasks/cc/vision/pose_landmarker/pose_landmarker.h"
#include "mediapipe/tasks/cc/vision/pose_landmarker/pose_landmarks_connections.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace vision {
namespace {

struct CalibrationData {
    cv::Mat camera_matrix;
    cv::Mat distortion_coefficients;
};

std::optional<CalibrationData> loadCalibration(
    const std::optional<std::filesystem::path>& path) {
    if (!path.has_value()) {
        return std::nullopt;
    }
    cv::FileStorage input(path->string(), cv::FileStorage::READ);
    if (!input.isOpened()) {
        throw std::runtime_error("Could not open calibration file: " + path->string());
    }
    CalibrationData data;
    input["camera_matrix"] >> data.camera_matrix;
    input["distortion_coefficients"] >> data.distortion_coefficients;
    if (data.camera_matrix.empty() || data.distortion_coefficients.empty()) {
        throw std::runtime_error("Calibration file is missing camera parameters: " +
                                 path->string());
    }
    return data;
}

bool openInput(const std::string& input, cv::VideoCapture& capture) {
    if (input == "camera") {
        return capture.open(0);
    }
    constexpr const char* prefix = "camera:";
    if (input.rfind(prefix, 0) == 0) {
        return capture.open(std::stoi(input.substr(std::char_traits<char>::length(prefix))));
    }
    return capture.open(input);
}

template <typename LandmarkList>
std::array<PoseLandmark, 33> convertLandmarks(const LandmarkList& input) {
    std::array<PoseLandmark, 33> output{};
    const std::size_t count = std::min(output.size(), input.size());
    for (std::size_t i = 0; i < count; ++i) {
        output[i] = {
            input[i].x,
            input[i].y,
            input[i].visibility.value_or(1.0F),
            input[i].presence.value_or(1.0F)};
    }
    return output;
}

template <typename Landmark>
bool isVisible(const Landmark& landmark) {
    return landmark.visibility.value_or(1.0F) >= SquatCounter::kMinimumConfidence &&
           landmark.presence.value_or(1.0F) >= SquatCounter::kMinimumConfidence &&
           landmark.x >= 0.0F && landmark.x <= 1.0F &&
           landmark.y >= 0.0F && landmark.y <= 1.0F;
}

template <typename Landmark>
cv::Point pixelPoint(const Landmark& landmark, const cv::Mat& frame) {
    return {
        static_cast<int>(landmark.x * (frame.cols - 1)),
        static_cast<int>(landmark.y * (frame.rows - 1))};
}

template <typename LandmarkList>
void drawPose(const LandmarkList& landmarks, cv::Mat& frame) {
    for (const auto& connection :
         mediapipe::tasks::vision::pose_landmarker::kPoseLandmarksConnections) {
        const auto& start = landmarks[connection[0]];
        const auto& end = landmarks[connection[1]];
        if (isVisible(start) && isVisible(end)) {
            cv::line(frame, pixelPoint(start, frame), pixelPoint(end, frame),
                     cv::Scalar(0, 220, 0), 2, cv::LINE_AA);
        }
    }
    for (const auto& landmark : landmarks) {
        if (isVisible(landmark)) {
            cv::circle(frame, pixelPoint(landmark, frame), 4,
                       cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA);
        }
    }
}

void drawSquatHud(const SquatMetrics& metrics, cv::Mat& frame) {
    cv::rectangle(frame, cv::Rect(12, 12, 310, 105), cv::Scalar(20, 20, 20),
                  cv::FILLED);
    cv::putText(frame, "Squats: " + std::to_string(metrics.repetitions),
                cv::Point(25, 48), cv::FONT_HERSHEY_SIMPLEX, 0.9,
                cv::Scalar(255, 255, 255), 2, cv::LINE_AA);

    std::ostringstream angles;
    angles << std::fixed << std::setprecision(1) << "Knee angles: ";
    if (metrics.left_knee_angle.has_value()) {
        angles << "L " << *metrics.left_knee_angle;
    } else {
        angles << "L --";
    }
    if (metrics.right_knee_angle.has_value()) {
        angles << "  R " << *metrics.right_knee_angle;
    } else {
        angles << "  R --";
    }
    cv::putText(frame, angles.str(), cv::Point(25, 82),
                cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(220, 220, 220),
                1, cv::LINE_AA);
    cv::putText(frame, "down <120 deg | up >155 deg", cv::Point(25, 105),
                cv::FONT_HERSHEY_SIMPLEX, 0.42, cv::Scalar(180, 180, 180),
                1, cv::LINE_AA);
}

}  // namespace

int runPoseAnalysis(const PoseAnalysisOptions& options) {
    if (!std::filesystem::is_regular_file(options.model_path)) {
        std::cerr << "MediaPipe model does not exist: " << options.model_path << '\n';
        return 1;
    }

    cv::VideoCapture capture;
    if (!openInput(options.input, capture)) {
        std::cerr << "Could not open camera or video input: " << options.input << '\n';
        return 1;
    }

    const auto calibration = loadCalibration(options.calibration_path);
    auto landmarker_options = std::make_unique<
        mediapipe::tasks::vision::pose_landmarker::PoseLandmarkerOptions>();
    landmarker_options->base_options.model_asset_path = options.model_path.string();
    landmarker_options->base_options.delegate =
        mediapipe::tasks::core::BaseOptions::CPU;
    landmarker_options->running_mode =
        mediapipe::tasks::vision::core::RunningMode::VIDEO;
    landmarker_options->num_poses = 1;

    auto landmarker_result =
        mediapipe::tasks::vision::pose_landmarker::PoseLandmarker::Create(
            std::move(landmarker_options));
    if (!landmarker_result.ok()) {
        std::cerr << "Could not create Pose Landmarker: "
                  << landmarker_result.status().ToString() << '\n';
        return 1;
    }
    auto landmarker = std::move(landmarker_result.value());

    double frames_per_second = capture.get(cv::CAP_PROP_FPS);
    if (frames_per_second <= 0.0) {
        frames_per_second = 30.0;
    }
    cv::VideoWriter writer;
    SquatCounter squat_counter;
    cv::Mat captured_frame;
    std::int64_t frame_index = 0;
    bool paused = false;

    while (true) {
        if (!paused && !capture.read(captured_frame)) {
            break;
        }

        if (!paused) {
            cv::Mat frame;
            if (calibration.has_value()) {
                cv::undistort(captured_frame, frame, calibration->camera_matrix,
                              calibration->distortion_coefficients);
            } else {
                frame = captured_frame.clone();
            }

            cv::Mat rgb_frame;
            cv::cvtColor(frame, rgb_frame, cv::COLOR_BGR2RGB);
            auto image_frame = std::make_shared<mediapipe::ImageFrame>(
                mediapipe::ImageFormat::SRGB, frame.cols, frame.rows);
            cv::Mat image_frame_view =
                mediapipe::formats::MatView(image_frame.get());
            rgb_frame.copyTo(image_frame_view);

            const std::int64_t timestamp_ms = static_cast<std::int64_t>(
                frame_index * 1000.0 / frames_per_second);
            auto detection = landmarker->DetectForVideo(
                mediapipe::Image(image_frame), timestamp_ms);
            if (!detection.ok()) {
                std::cerr << "Pose detection failed: "
                          << detection.status().ToString() << '\n';
                return 1;
            }

            SquatMetrics metrics;
            metrics.repetitions = squat_counter.repetitions();
            if (!detection->pose_landmarks.empty() &&
                detection->pose_landmarks[0].landmarks.size() >= 33) {
                const auto& landmarks = detection->pose_landmarks[0].landmarks;
                metrics = squat_counter.update(convertLandmarks(landmarks));
                drawPose(landmarks, frame);
            }
            drawSquatHud(metrics, frame);

            if (options.output_video_path.has_value()) {
                if (!writer.isOpened()) {
                    const auto parent = options.output_video_path->parent_path();
                    if (!parent.empty()) {
                        std::filesystem::create_directories(parent);
                    }
                    writer.open(
                        options.output_video_path->string(),
                        cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                        frames_per_second, frame.size());
                    if (!writer.isOpened()) {
                        std::cerr << "Could not create output video: "
                                  << *options.output_video_path << '\n';
                        return 1;
                    }
                }
                writer.write(frame);
            }

            captured_frame = frame;
            ++frame_index;
        }

        if (options.display) {
            cv::imshow("Vision Artificial - pose and squats", captured_frame);
            const int key = cv::waitKey(paused ? 0 : 1);
            if (key == 27 || key == 'q' || key == 'Q') {
                break;
            }
            if (key == ' ') {
                paused = !paused;
            } else if (key == 'r' || key == 'R') {
                squat_counter.reset();
            }
        }
    }

    std::cout << "Pose analysis complete. Squats counted: "
              << squat_counter.repetitions() << '\n';
    return 0;
}

}  // namespace vision
