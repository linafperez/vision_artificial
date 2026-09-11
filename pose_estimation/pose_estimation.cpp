#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <opencv2/core.hpp>    // Provides core OpenCV structures such as cv::Mat for storing image and video frames
#include <opencv2/videoio.hpp> // Provides video input/output tools such as cv::VideoCapture for opening and reading videos
#include <opencv2/highgui.hpp> // Provides imshow() and waitKey() for displaying frames.
#include <opencv2/imgproc.hpp> // Provides color conversion and drawing tools
#include "mediapipe/framework/formats/image.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/tasks/cc/core/base_options.h"
#include "mediapipe/tasks/cc/vision/core/running_mode.h"
#include "mediapipe/tasks/cc/vision/pose_landmarker/pose_landmarker.h"
#include "mediapipe/tasks/cc/vision/pose_landmarker/pose_landmarks_connections.h"

int main(){

    cv::VideoCapture cap("input_videos/walking.mp4");

    if (!cap.isOpened()){

        std::cerr << "Error: could not open video." << std::endl;
        return -1;
    }

    // Configure Pose Landmarker
    auto options = std::make_unique<mediapipe::tasks::vision::pose_landmarker::PoseLandmarkerOptions>();
    options->base_options.model_asset_path = "models/pose_landmarker.task";
    options->base_options.delegate = mediapipe::tasks::core::BaseOptions::CPU;
    options->running_mode = mediapipe::tasks::vision::core::RunningMode::VIDEO;
    options->num_poses = 1;

    auto pose_landmarker_result = mediapipe::tasks::vision::pose_landmarker::PoseLandmarker::Create(std::move(options));

    if (!pose_landmarker_result.ok()){

        std::cerr << "Error: could not create Pose Landmarker: "
                  << pose_landmarker_result.status().ToString() << std::endl;
        return -1;
    }

    auto pose_landmarker = std::move(pose_landmarker_result.value());

    cv::Mat frame;
    cv::Mat rgb_frame;
    bool paused = false;
    int64_t timestamp_ms = 0;

    while (true){

        // Only read a new frame when the video is not paused
        if (!paused && !cap.read(frame)) break;  // End of video

        if (!paused){

            // Convert the frame from BGR to RGB
            cv::cvtColor(frame, rgb_frame, cv::COLOR_BGR2RGB);

            auto image_frame = std::make_shared<mediapipe::ImageFrame>(
                mediapipe::ImageFormat::SRGB, frame.cols, frame.rows);
            cv::Mat image_frame_mat = mediapipe::formats::MatView(image_frame.get());
            rgb_frame.copyTo(image_frame_mat);

            mediapipe::Image image(image_frame);
            auto result = pose_landmarker->DetectForVideo(image, timestamp_ms++);

            if (!result.ok()){

                std::cerr << "Error: pose detection failed: "
                          << result.status().ToString() << std::endl;
                return -1;
            }

            if (!result->pose_landmarks.empty()){

                const auto& landmarks = result->pose_landmarks[0].landmarks;

                // Draw pose connections
                for (const auto& connection : mediapipe::tasks::vision::pose_landmarker::kPoseLandmarksConnections){

                    const auto& start = landmarks[connection[0]];
                    const auto& end = landmarks[connection[1]];

                    if (start.visibility.value_or(1.0f) < 0.5f ||
                        start.presence.value_or(1.0f) < 0.5f ||
                        end.visibility.value_or(1.0f) < 0.5f ||
                        end.presence.value_or(1.0f) < 0.5f ||
                        start.x < 0.0f || start.x > 1.0f ||
                        start.y < 0.0f || start.y > 1.0f ||
                        end.x < 0.0f || end.x > 1.0f ||
                        end.y < 0.0f || end.y > 1.0f) continue;

                    cv::Point start_point(
                        static_cast<int>(start.x * (frame.cols - 1)),
                        static_cast<int>(start.y * (frame.rows - 1)));
                    cv::Point end_point(
                        static_cast<int>(end.x * (frame.cols - 1)),
                        static_cast<int>(end.y * (frame.rows - 1)));

                    cv::line(frame, start_point, end_point, cv::Scalar(0, 255, 0), 2);
                }

                // Draw pose landmarks
                for (const auto& landmark : landmarks){

                    if (landmark.visibility.value_or(1.0f) < 0.5f ||
                        landmark.presence.value_or(1.0f) < 0.5f ||
                        landmark.x < 0.0f || landmark.x > 1.0f ||
                        landmark.y < 0.0f || landmark.y > 1.0f) continue;

                    cv::Point point(
                        static_cast<int>(landmark.x * (frame.cols - 1)),
                        static_cast<int>(landmark.y * (frame.rows - 1)));

                    cv::circle(frame, point, 4, cv::Scalar(0, 0, 255), -1);
                }
            }
        }

        // Display the current frame
        cv::imshow("Video", frame);

        // Check keyboard input
        int key = cv::waitKey(30);

        // Space bar: pause/play
        if (key == 32){
            
            paused = !paused;

        } else if (key == 27){ // Escape: exit

            break;
        }
    }

    return 0;
}
