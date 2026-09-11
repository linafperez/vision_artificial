load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

cc_binary(
    name = "executable",
    srcs = ["pose_estimation.cpp"],
    deps = [
        "//mediapipe/framework/formats:image",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/formats:image_frame_opencv",

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
