#include "calibration.h"
#include "pose_analysis.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printUsage(const char* program) {
    std::cout
        << "Integrated camera calibration and pose-analysis application\n\n"
        << "Calibration:\n"
        << "  " << program
        << " calibrate --images <directory> --output <directory>"
           " [--board-cols 8] [--board-rows 5] [--square-mm 26]\n\n"
        << "Pose analysis:\n"
        << "  " << program
        << " pose [--input camera|camera:<index>|<video>]"
           " [--model <task-file>] [--calibration <yaml>]"
           " [--output <mp4>] [--no-display]\n";
}

std::string requireValue(int argc, char* argv[], int& index) {
    if (index + 1 >= argc) {
        throw std::invalid_argument(std::string("Missing value for ") + argv[index]);
    }
    return argv[++index];
}

int runCalibrationCommand(int argc, char* argv[]) {
    vision::CalibrationOptions options;

    for (int i = 2; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--images") {
            options.image_directory = requireValue(argc, argv, i);
        } else if (argument == "--output") {
            options.output_directory = requireValue(argc, argv, i);
        } else if (argument == "--board-cols") {
            options.board_columns = std::stoi(requireValue(argc, argv, i));
        } else if (argument == "--board-rows") {
            options.board_rows = std::stoi(requireValue(argc, argv, i));
        } else if (argument == "--square-mm") {
            options.square_size_mm = std::stof(requireValue(argc, argv, i));
        } else {
            throw std::invalid_argument("Unknown calibration option: " + argument);
        }
    }

    if (options.image_directory.empty() || options.output_directory.empty()) {
        throw std::invalid_argument("Calibration requires --images and --output");
    }
    return vision::runCalibration(options);
}

int runPoseCommand(int argc, char* argv[]) {
    vision::PoseAnalysisOptions options;

    for (int i = 2; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--input") {
            options.input = requireValue(argc, argv, i);
        } else if (argument == "--model") {
            options.model_path = requireValue(argc, argv, i);
        } else if (argument == "--calibration") {
            options.calibration_path = requireValue(argc, argv, i);
        } else if (argument == "--output") {
            options.output_video_path = requireValue(argc, argv, i);
        } else if (argument == "--no-display") {
            options.display = false;
        } else {
            throw std::invalid_argument("Unknown pose option: " + argument);
        }
    }
    return vision::runPoseAnalysis(options);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        const std::string command = argv[1];
        if (command == "calibrate") {
            return runCalibrationCommand(argc, argv);
        }
        if (command == "pose") {
            return runPoseCommand(argc, argv);
        }
        if (command == "--help" || command == "-h" || command == "help") {
            printUsage(argv[0]);
            return 0;
        }

        throw std::invalid_argument("Unknown command: " + command);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        printUsage(argv[0]);
        return 1;
    }
}
