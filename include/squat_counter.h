#pragma once

#include <array>
#include <optional>

namespace vision {

struct PoseLandmark {
    float x = 0.0F;
    float y = 0.0F;
    float visibility = 0.0F;
    float presence = 0.0F;
};

struct SquatMetrics {
    int repetitions = 0;
    std::optional<float> left_knee_angle;
    std::optional<float> right_knee_angle;
};

class SquatCounter {
public:
    static constexpr float kDownThresholdDegrees = 120.0F;
    static constexpr float kUpThresholdDegrees = 155.0F;
    static constexpr float kMinimumConfidence = 0.5F;

    SquatMetrics update(const std::array<PoseLandmark, 33>& landmarks);
    int repetitions() const;
    void reset();

private:
    enum class Phase { Up, Down };

    struct SideState {
        Phase phase = Phase::Up;
        int repetitions = 0;
    };

    static std::optional<float> kneeAngle(
        const PoseLandmark& hip,
        const PoseLandmark& knee,
        const PoseLandmark& ankle);
    static void updateSide(SideState& side, const std::optional<float>& angle);

    SideState left_;
    SideState right_;
};

}  // namespace vision
