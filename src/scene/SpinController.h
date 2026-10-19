#pragma once

#include <deque>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace scene {

// Cube orientation with automatic spin and drag-and-flick control.
// Positions are window pixels (y grows downward); times are seconds.
class SpinController {
public:
    static constexpr float kDefaultSpeed = 1.5707963f; // 90 degrees per second
    static constexpr float kDragRadiansPerPixel = 0.01f;
    static constexpr float kFlickMinSpeed = 50.0f; // pixels per second
    static constexpr double kFlickWindow = 0.1;    // seconds

    SpinController();

    void update(float dt);

    void setSpinning(bool spinning) { spinning_ = spinning; }
    bool isSpinning() const { return spinning_; }

    void beginDrag(glm::vec2 pos, double time);
    void dragTo(glm::vec2 pos, double time);
    void endDrag(double time);
    bool isDragging() const { return dragging_; }

    glm::quat orientation() const { return orientation_; }
    glm::vec3 axis() const { return axis_; }
    float speed() const { return speed_; }

private:
    struct Sample {
        glm::vec2 pos;
        double time;
    };

    static glm::vec3 screenAxis(glm::vec2 direction);
    void dropSamplesBefore(double time);

    glm::quat orientation_{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 axis_;
    float speed_ = kDefaultSpeed;
    bool spinning_ = true;
    bool dragging_ = false;
    std::deque<Sample> samples_;
};

} // namespace scene
