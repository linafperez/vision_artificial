load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "calibration",
    srcs = ["src/calibration.cpp"],
    hdrs = ["include/calibration.h"],
    copts = ["-std=c++17"],
    includes = ["include"],
    deps = [
        "//mediapipe/framework/port:opencv_calib3d",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgcodecs",
        "//mediapipe/framework/port:opencv_imgproc",
    ],
)
cc_library(
    name = "squat_counter",
    srcs = ["src/squat_counter.cpp"],
    hdrs = ["include/squat_counter.h"],
    copts = ["-std=c++17"],
    includes = ["include"],
)

cc_library(
    name = "pose_analysis",
    srcs = ["src/pose_analysis.cpp"],
    hdrs = ["include/pose_analysis.h"],
    copts = ["-std=c++17"],
    includes = ["include"],
    deps = [
        ":squat_counter",
        "//mediapipe/framework/formats:image",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/formats:image_frame_opencv",
        "//mediapipe/framework/port:opencv_calib3d",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_highgui",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_videoio",
        "//mediapipe/tasks/cc/core:base_options",
        "//mediapipe/tasks/cc/vision/core:running_mode",
        "//mediapipe/tasks/cc/vision/pose_landmarker:pose_landmarker",
        "//mediapipe/tasks/cc/vision/pose_landmarker:pose_landmarks_connections",
    ],
)

cc_binary(
    name = "vision_app",
    srcs = ["src/main.cpp"],
    copts = ["-std=c++17"],
    data = ["models/pose_landmarker.task"],
    deps = [
        ":calibration",
        ":pose_analysis",
    ],
)
