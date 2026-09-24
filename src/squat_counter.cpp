#include "squat_counter.h"

#include <algorithm>
#include <cmath>

namespace vision {
namespace {

constexpr std::size_t kLeftHip = 23;
constexpr std::size_t kRightHip = 24;
constexpr std::size_t kLeftKnee = 25;
constexpr std::size_t kRightKnee = 26;
constexpr std::size_t kLeftAnkle = 27;
constexpr std::size_t kRightAnkle = 28;
constexpr float kRadiansToDegrees = 57.29577951308232F;

float confidence(const PoseLandmark& landmark) {
    return std::min(landmark.visibility, landmark.presence);
}

}  // namespace

std::optional<float> SquatCounter::kneeAngle(
    const PoseLandmark& hip,
    const PoseLandmark& knee,
    const PoseLandmark& ankle) {
    if (confidence(hip) < kMinimumConfidence ||
        confidence(knee) < kMinimumConfidence ||
        confidence(ankle) < kMinimumConfidence) {
        return std::nullopt;
    }

    const float thigh_x = hip.x - knee.x;
    const float thigh_y = hip.y - knee.y;
    const float shin_x = ankle.x - knee.x;
    const float shin_y = ankle.y - knee.y;
    const float thigh_length = std::hypot(thigh_x, thigh_y);
    const float shin_length = std::hypot(shin_x, shin_y);

    if (thigh_length < 1.0e-6F || shin_length < 1.0e-6F) {
        return std::nullopt;
    }

    const float cosine = std::clamp(
        (thigh_x * shin_x + thigh_y * shin_y) /
            (thigh_length * shin_length),
        -1.0F,
        1.0F);
    return std::acos(cosine) * kRadiansToDegrees;
}

void SquatCounter::updateSide(
    SideState& side,
    const std::optional<float>& angle) {
    if (!angle.has_value()) {
        return;
    }

    if (side.phase == Phase::Up && *angle < kDownThresholdDegrees) {
        side.phase = Phase::Down;
    } else if (side.phase == Phase::Down && *angle > kUpThresholdDegrees) {
        side.phase = Phase::Up;
        ++side.repetitions;
    }
}

SquatMetrics SquatCounter::update(
    const std::array<PoseLandmark, 33>& landmarks) {
    const auto left_angle = kneeAngle(
        landmarks[kLeftHip], landmarks[kLeftKnee], landmarks[kLeftAnkle]);
    const auto right_angle = kneeAngle(
        landmarks[kRightHip], landmarks[kRightKnee], landmarks[kRightAnkle]);

    updateSide(left_, left_angle);
    updateSide(right_, right_angle);

    return {repetitions(), left_angle, right_angle};
}

int SquatCounter::repetitions() const {
    return std::max(left_.repetitions, right_.repetitions);
}

void SquatCounter::reset() {
    left_ = {};
    right_ = {};
}

}  // namespace vision
