#include "calibration.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace vision {
namespace {

namespace fs = std::filesystem;

bool isSupportedImage(const fs::path& path) {
    static const std::set<std::string> extensions = {
        ".bmp", ".jpeg", ".jpg", ".png", ".tif", ".tiff"};
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return extensions.count(extension) != 0;
}

std::vector<fs::path> listImages(const fs::path& directory) {
    std::vector<fs::path> images;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file() && isSupportedImage(entry.path())) {
            images.push_back(entry.path());
        }
    }
    std::sort(images.begin(), images.end());
    return images;
}

std::vector<cv::Point3f> makeObjectPoints(
    const cv::Size board_size,
    const float square_size_mm) {
    std::vector<cv::Point3f> points;
    points.reserve(static_cast<std::size_t>(board_size.area()));
    for (int row = 0; row < board_size.height; ++row) {
        for (int column = 0; column < board_size.width; ++column) {
            points.emplace_back(
                column * square_size_mm, row * square_size_mm, 0.0F);
        }
    }
    return points;
}

double computeReprojectionError(
    const std::vector<std::vector<cv::Point3f>>& object_points,
    const std::vector<std::vector<cv::Point2f>>& image_points,
    const std::vector<cv::Mat>& rotation_vectors,
    const std::vector<cv::Mat>& translation_vectors,
    const cv::Mat& camera_matrix,
    const cv::Mat& distortion_coefficients,
    std::vector<double>& per_view_errors) {
    double squared_error = 0.0;
    std::size_t point_count = 0;
    per_view_errors.clear();

    for (std::size_t i = 0; i < object_points.size(); ++i) {
        std::vector<cv::Point2f> projected_points;
        cv::projectPoints(
            object_points[i], rotation_vectors[i], translation_vectors[i],
            camera_matrix, distortion_coefficients, projected_points);
        const double error = cv::norm(
            image_points[i], projected_points, cv::NORM_L2);
        per_view_errors.push_back(
            std::sqrt(error * error / object_points[i].size()));
        squared_error += error * error;
        point_count += object_points[i].size();
    }

    return std::sqrt(squared_error / static_cast<double>(point_count));
}

}  // namespace

int runCalibration(const CalibrationOptions& options) {
    if (!fs::is_directory(options.image_directory)) {
        std::cerr << "Calibration image directory does not exist: "
                  << options.image_directory << '\n';
        return 1;
    }
    if (options.board_columns <= 0 || options.board_rows <= 0 ||
        options.square_size_mm <= 0.0F) {
        std::cerr << "Board dimensions and square size must be positive.\n";
        return 1;
    }

    const auto images = listImages(options.image_directory);
    if (images.empty()) {
        std::cerr << "No supported calibration images were found.\n";
        return 1;
    }

    fs::create_directories(options.output_directory / "detected_corners");
    const cv::Size board_size(options.board_columns, options.board_rows);
    const auto board_points =
        makeObjectPoints(board_size, options.square_size_mm);
    std::vector<std::vector<cv::Point2f>> image_points;
    std::vector<std::vector<cv::Point3f>> object_points;
    std::vector<std::string> successful_images;
    cv::Size image_size;

    std::cout << "Inspecting " << images.size() << " calibration images...\n";
    for (const auto& path : images) {
        cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
            std::cout << "Skipped unreadable image: " << path.filename() << '\n';
            continue;
        }
        if (image_size.empty()) {
            image_size = image.size();
        } else if (image.size() != image_size) {
            std::cerr << "All calibration images must have the same resolution; "
                      << path.filename() << " differs.\n";
            return 1;
        }

        cv::Mat gray;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        std::vector<cv::Point2f> corners;
        const bool found = cv::findChessboardCorners(
            gray, board_size, corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);
        if (!found) {
            std::cout << "Chessboard not detected: " << path.filename() << '\n';
            continue;
        }

        cv::cornerSubPix(
            gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
            cv::TermCriteria(
                cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER,
                30, 0.001));
        image_points.push_back(corners);
        object_points.push_back(board_points);
        successful_images.push_back(path.string());

        cv::drawChessboardCorners(image, board_size, corners, true);
        const fs::path corner_path =
            options.output_directory / "detected_corners" /
            (path.stem().string() + "_corners.jpg");
        cv::imwrite(corner_path.string(), image);
    }

    if (image_points.size() < 2) {
        std::cerr << "At least two valid chessboard views are required.\n";
        return 1;
    }
    if (image_points.size() < 10) {
        std::cout << "Warning: fewer than 10 valid views were detected.\n";
    }

    cv::Mat camera_matrix;
    cv::Mat distortion_coefficients;
    std::vector<cv::Mat> rotation_vectors;
    std::vector<cv::Mat> translation_vectors;
    const int flags =
        cv::CALIB_FIX_PRINCIPAL_POINT | cv::CALIB_ZERO_TANGENT_DIST;
    const double calibration_rms = cv::calibrateCamera(
        object_points, image_points, image_size, camera_matrix,
        distortion_coefficients, rotation_vectors, translation_vectors, flags);

    std::vector<double> per_view_errors;
    const double reprojection_error = computeReprojectionError(
        object_points, image_points, rotation_vectors, translation_vectors,
        camera_matrix, distortion_coefficients, per_view_errors);

    const fs::path output_file =
        options.output_directory / "calibration_results.yaml";
    cv::FileStorage output(output_file.string(), cv::FileStorage::WRITE);
    if (!output.isOpened()) {
        std::cerr << "Could not write calibration file: " << output_file << '\n';
        return 1;
    }
    output << "image_width" << image_size.width;
    output << "image_height" << image_size.height;
    output << "board_columns" << options.board_columns;
    output << "board_rows" << options.board_rows;
    output << "square_size_mm" << options.square_size_mm;
    output << "camera_matrix" << camera_matrix;
    output << "distortion_coefficients" << distortion_coefficients;
    output << "calibration_rms_error" << calibration_rms;
    output << "overall_reprojection_error" << reprojection_error;
    output << "rotation_vectors" << rotation_vectors;
    output << "translation_vectors" << translation_vectors;
    output << "successful_images" << "[";
    for (const auto& path : successful_images) {
        output << path;
    }
    output << "]";
    output << "per_view_reprojection_errors" << "[";
    for (const double error : per_view_errors) {
        output << error;
    }
    output << "]";
    output.release();

    std::cout << "Calibration complete: " << image_points.size()
              << " valid views, RMS " << calibration_rms
              << " px, reprojection error " << reprojection_error << " px.\n"
              << "Saved to " << output_file << '\n';
    return 0;
}

}  // namespace vision
