#include "scene/Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace scene {

void Camera::zoom(float notches)
{
    distance_ = std::clamp(distance_ * std::pow(kZoomFactorPerNotch, notches), kMinDistance, kMaxDistance);
}

void Camera::setViewport(int width, int height)
{
    aspect_ = (width > 0 && height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
}

glm::mat4 Camera::view() const
{
    return glm::lookAt(glm::vec3(0.0f, 0.0f, distance_), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::projection() const
{
    glm::mat4 p = glm::perspectiveRH_ZO(glm::radians(kFovYDegrees), aspect_, kNear, kFar);
    p[1][1] *= -1.0f; // Vulkan framebuffer Y points down
    return p;
}

} // namespace scene
