#include "scene/SpinController.h"

namespace scene {

SpinController::SpinController()
    : axis_(glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)))
{
}

void SpinController::update(float dt)
{
    if (!spinning_ || dragging_) {
        return;
    }
    orientation_ = glm::normalize(glm::angleAxis(speed_ * dt, axis_) * orientation_);
}

void SpinController::beginDrag(glm::vec2 pos, double time)
{
    dragging_ = true;
    samples_.clear();
    samples_.push_back({pos, time});
}

void SpinController::dragTo(glm::vec2 pos, double time)
{
    if (!dragging_) {
        return;
    }
    const glm::vec2 delta = pos - samples_.back().pos;
    const float distance = glm::length(delta);
    if (distance > 0.0f) {
        const glm::quat step = glm::angleAxis(distance * kDragRadiansPerPixel, screenAxis(delta));
        orientation_ = glm::normalize(step * orientation_);
    }
    samples_.push_back({pos, time});
    dropSamplesBefore(time - kFlickWindow);
}

void SpinController::endDrag(double time)
{
    if (!dragging_) {
        return;
    }
    dragging_ = false;
    dropSamplesBefore(time - kFlickWindow);
    if (samples_.size() >= 2) {
        const double span = time - samples_.front().time;
        if (span > 0.0) {
            const glm::vec2 velocity =
                (samples_.back().pos - samples_.front().pos) / static_cast<float>(span);
            if (glm::length(velocity) > kFlickMinSpeed) {
                axis_ = screenAxis(velocity);
            }
        }
    }
    samples_.clear();
}

// A drag along screen direction (dx, dy) turns the cube about world axis (dy, dx, 0):
// right -> +Y, down -> +X (screen y grows downward).
glm::vec3 SpinController::screenAxis(glm::vec2 direction)
{
    return glm::normalize(glm::vec3(direction.y, direction.x, 0.0f));
}

void SpinController::dropSamplesBefore(double time)
{
    while (!samples_.empty() && samples_.front().time < time) {
        samples_.pop_front();
    }
}

} // namespace scene
