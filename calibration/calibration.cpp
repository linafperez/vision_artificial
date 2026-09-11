#include <opencv2/opencv.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

namespace fs = std::filesystem;

// Looks for an image with the specified index using common image extensions
std::string findImagePath(const fs::path& folder, int index)
{
    const std::vector<std::string> extensions = {
        ".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff",
        ".JPG", ".JPEG", ".PNG", ".BMP", ".TIF", ".TIFF"
    };

    for (const std::string& extension : extensions)
    {
        fs::path imagePath = folder / (std::to_string(index) + extension);

        if (fs::exists(imagePath))
        {
            return imagePath.string();
        }
    }

    return "";
}


// Creates the physical 3D coordinates of the internal chessboard corners.
std::vector<cv::Point3f> createChessboardObjectPoints(cv::Size boardSize, float squareSize)
{
    std::vector<cv::Point3f> objectPoints;

    for (int row = 0; row < boardSize.height; ++row)
    {
        for (int column = 0; column < boardSize.width; ++column)
        {
            objectPoints.emplace_back(
                column * squareSize,
                row * squareSize,
                0.0f
            );
        }
    }

    return objectPoints;
}


// Computes the reprojection error for every calibration image.
double computeReprojectionErrors(
    const std::vector<std::vector<cv::Point3f>>& objectPoints,
    const std::vector<std::vector<cv::Point2f>>& imagePoints,
    const std::vector<cv::Mat>& rotationVectors,
    const std::vector<cv::Mat>& translationVectors,
    const cv::Mat& cameraMatrix,
    const cv::Mat& distortionCoefficients,
    std::vector<double>& perViewErrors
)
{
    double totalSquaredError = 0.0;
    size_t totalPoints = 0;

    perViewErrors.clear();

    for (size_t i = 0; i < objectPoints.size(); ++i)
    {
        std::vector<cv::Point2f> projectedPoints;

        cv::projectPoints(
            objectPoints[i],
            rotationVectors[i],
            translationVectors[i],
            cameraMatrix,
            distortionCoefficients,
            projectedPoints
        );

        double error = cv::norm(
            imagePoints[i],
            projectedPoints,
            cv::NORM_L2
        );

        double viewError = std::sqrt(
            (error * error) / objectPoints[i].size()
        );

        perViewErrors.push_back(viewError);

        totalSquaredError += error * error;
        totalPoints += objectPoints[i].size();
    }

    return std::sqrt(totalSquaredError / totalPoints);
}


int main(int argc, char* argv[])
{
    
    //Expected command:
    // ./calibrate_camera <image_folder> <board_columns> <board_rows> <square_size (in mm)>

    if (argc != 5)
    {
        std::cerr
            << "Usage:\n"
            << "  " << argv[0]
            << " <image_folder> <board_columns> <board_rows> <square_size (in mm)>\n\n";

        return 1;
    }

    const fs::path imageFolder = argv[1];

    const int boardColumns = std::stoi(argv[2]);
    const int boardRows = std::stoi(argv[3]);
    const float squareSize = std::stof(argv[4]);

    const int firstImage = 1;
    const int lastImage = 35;

    if (!fs::exists(imageFolder) || !fs::is_directory(imageFolder))
    {
        std::cerr << "Error: the specified image folder does not exist.\n";
        return 1;
    }

    std::string folderName = imageFolder.filename().string();
    fs::path resultsFolder = imageFolder / ("results_" + folderName);
    fs::create_directories(resultsFolder);

    if (boardColumns <= 0 || boardRows <= 0)
    {
        std::cerr << "Error: chessboard dimensions must be positive.\n";
        return 1;
    }

    if (squareSize <= 0.0f)
    {
        std::cerr << "Error: square size must be greater than zero.\n";
        return 1;
    }

    const cv::Size boardSize(boardColumns, boardRows);

    /*
     * These vectors contain the information required by calibrateCamera().
     *
     * imagePoints:
     *   Detected chessboard corners in image coordinates (pixels).
     *
     * objectPoints:
     *   Physical chessboard corner coordinates in the chessboard coordinate
     *   system.
     */
    std::vector<std::vector<cv::Point2f>> imagePoints;
    std::vector<std::vector<cv::Point3f>> objectPoints;

    std::vector<std::string> successfulImages;

    cv::Size imageSize;

    std::vector<cv::Point3f> chessboardPoints =
        createChessboardObjectPoints(boardSize, squareSize);

    fs::path detectedFolder = resultsFolder / "detected_corners";
    fs::create_directories(detectedFolder);

    std::cout << "\nSearching for calibration images...\n\n";

    for (int imageIndex = firstImage; imageIndex <= lastImage; ++imageIndex)
    {
        std::string imagePath = findImagePath(imageFolder, imageIndex);

        if (imagePath.empty())
        {
            std::cout
                << "[" << imageIndex << "/35] "
                << "Image not found. Skipping.\n";

            continue;
        }

        cv::Mat image = cv::imread(imagePath);

        if (image.empty())
        {
            std::cout
                << "[" << imageIndex << "/35] "
                << "Could not read image. Skipping.\n";

            continue;
        }

        if (imageSize.empty())
        {
            imageSize = image.size();
        }
        else if (image.size() != imageSize)
        {
            std::cerr
                << "\nError: all calibration images must have the same resolution.\n"
                << "Image " << imageIndex << " has a different size.\n";

            return 1;
        }

        cv::Mat grayImage;

        cv::cvtColor(
            image,
            grayImage,
            cv::COLOR_BGR2GRAY
        );

        std::vector<cv::Point2f> corners;

        bool found = cv::findChessboardCorners(
            grayImage,
            boardSize,
            corners,
            cv::CALIB_CB_ADAPTIVE_THRESH |
            cv::CALIB_CB_NORMALIZE_IMAGE
        );

        if (!found)
        {
            std::cout
                << "[" << imageIndex << "/35] "
                << "Chessboard not detected. Skipping.\n";

            continue;
        }

        /*
         * Refine the detected corner positions to subpixel precision.
         */
        cv::cornerSubPix(
            grayImage,
            corners,
            cv::Size(11, 11),
            cv::Size(-1, -1),
            cv::TermCriteria(
                cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER,
                30,
                0.001
            )
        );

        imagePoints.push_back(corners);
        objectPoints.push_back(chessboardPoints);
        successfulImages.push_back(imagePath);

        /*
         * Save an image showing the corners that OpenCV detected.
         */
        cv::Mat visualization = image.clone();

        cv::drawChessboardCorners(
            visualization,
            boardSize,
            corners,
            found
        );

        fs::path detectedPath =
            detectedFolder /
            (std::to_string(imageIndex) + "_corners.jpg");

        cv::imwrite(
            detectedPath.string(),
            visualization
        );

        std::cout
            << "[" << imageIndex << "/35] "
            << "Chessboard detected successfully. "
            << corners.size() << " corners found.\n";
    }

    std::cout
        << "\nSuccessful calibration views: "
        << imagePoints.size()
        << "\n";

    if (imagePoints.size() < 2)
    {
        std::cerr
            << "\nError: not enough valid chessboard views "
            << "were detected for calibration.\n";

        return 1;
    }

    if (imagePoints.size() < 10)
    {
        std::cout
            << "\nWarning: fewer than 10 valid views were detected. "
            << "The calibration may not be sufficiently robust.\n";
    }

    std::cout << "\nCalibrating camera...\n";

    cv::Mat cameraMatrix;
    cv::Mat distortionCoefficients;

    std::vector<cv::Mat> rotationVectors;
    std::vector<cv::Mat> translationVectors;

    /*
     * CALIB_FIX_PRINCIPAL_POINT:
     *   Keeps the principal point fixed at the image center.
     *
     * CALIB_ZERO_TANGENT_DIST:
     *   Assumes tangential distortion is zero.
     */
    int calibrationFlags =
        cv::CALIB_FIX_PRINCIPAL_POINT |
        cv::CALIB_ZERO_TANGENT_DIST;

    double calibrationRms = cv::calibrateCamera(
        objectPoints,
        imagePoints,
        imageSize,
        cameraMatrix,
        distortionCoefficients,
        rotationVectors,
        translationVectors,
        calibrationFlags
    );

    std::vector<double> perViewErrors;

    double overallReprojectionError = computeReprojectionErrors(
        objectPoints,
        imagePoints,
        rotationVectors,
        translationVectors,
        cameraMatrix,
        distortionCoefficients,
        perViewErrors
    );

    std::cout << "\nCalibration completed successfully.\n";

    std::cout
        << "\nImage resolution:\n"
        << imageSize.width
        << " x "
        << imageSize.height
        << " pixels\n";

    std::cout
        << "\nCamera intrinsic matrix:\n"
        << cameraMatrix
        << "\n";

    std::cout
        << "\nDistortion coefficients:\n"
        << distortionCoefficients
        << "\n";

    std::cout
        << "\nOpenCV calibration RMS error: "
        << calibrationRms
        << " pixels\n";

    std::cout
        << "Overall reprojection error: "
        << overallReprojectionError
        << " pixels\n";

    std::cout << "\nPer-view reprojection errors:\n";

    for (size_t i = 0; i < perViewErrors.size(); ++i)
    {
        std::cout
            << "View " << (i + 1)
            << ": "
            << perViewErrors[i]
            << " pixels\n";
    }

    /*
     * Save calibration parameters.
     */
    fs::path calibrationFile = resultsFolder / "calibration_results.yaml";

    cv::FileStorage fileStorage(
        calibrationFile.string(),
        cv::FileStorage::WRITE
    );

    fileStorage << "image_width" << imageSize.width;
    fileStorage << "image_height" << imageSize.height;

    fileStorage << "board_columns" << boardColumns;
    fileStorage << "board_rows" << boardRows;
    fileStorage << "square_size" << squareSize;

    fileStorage << "camera_matrix" << cameraMatrix;

    fileStorage
        << "distortion_coefficients"
        << distortionCoefficients;

    fileStorage
        << "calibration_rms_error"
        << calibrationRms;

    fileStorage
        << "overall_reprojection_error"
        << overallReprojectionError;

    fileStorage << "rotation_vectors" << rotationVectors;
    fileStorage << "translation_vectors" << translationVectors;

    fileStorage << "successful_images" << "[";

    for (const std::string& path : successfulImages)
    {
        fileStorage << path;
    }

    fileStorage << "]";

    fileStorage << "per_view_reprojection_errors" << "[";

    for (double error : perViewErrors)
    {
        fileStorage << error;
    }

    fileStorage << "]";

    fileStorage.release();

    return 0;
}