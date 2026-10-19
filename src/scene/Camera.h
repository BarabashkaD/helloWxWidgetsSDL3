#pragma once

#include <glm/glm.hpp>

namespace scene {

// Camera on the +Z axis looking at the origin, with clamped zoom.
class Camera {
public:
    static constexpr float kMinDistance = 1.8f;
    static constexpr float kMaxDistance = 10.0f;
    static constexpr float kDefaultDistance = 3.0f;
    static constexpr float kZoomFactorPerNotch = 0.9f;
    static constexpr float kFovYDegrees = 45.0f;
    static constexpr float kNear = 0.1f;
    static constexpr float kFar = 100.0f;

    // Positive notches move the camera closer; fractional notches are allowed.
    void zoom(float notches);
    float distance() const { return distance_; }

    void setViewport(int width, int height);
    float aspect() const { return aspect_; }

    glm::mat4 view() const;
    // Vulkan clip space: depth in [0, 1], world +Y up on screen.
    glm::mat4 projection() const;
    glm::mat4 viewProjection() const { return projection() * view(); }

private:
    float distance_ = kDefaultDistance;
    float aspect_ = 1.0f;
};

} // namespace scene
