#include <cmath>

#include <doctest/doctest.h>

#include "scene/Camera.h"

using scene::Camera;

TEST_CASE("default distance is 3")
{
    Camera camera;
    CHECK(camera.distance() == doctest::Approx(3.0f));
}

TEST_CASE("one notch in multiplies the distance by 0.9")
{
    Camera camera;
    camera.zoom(1.0f);
    CHECK(camera.distance() == doctest::Approx(2.7f));
}

TEST_CASE("one notch out divides the distance by 0.9")
{
    Camera camera;
    camera.zoom(-1.0f);
    CHECK(camera.distance() == doctest::Approx(3.0f / 0.9f));
}

TEST_CASE("fractional notches scale the exponent")
{
    Camera camera;
    camera.zoom(0.5f);
    CHECK(camera.distance() == doctest::Approx(3.0f * std::pow(0.9f, 0.5f)));
}

TEST_CASE("zoom clamps at both limits")
{
    Camera camera;
    camera.zoom(100.0f);
    CHECK(camera.distance() == doctest::Approx(Camera::kMinDistance));
    camera.zoom(-1000.0f);
    CHECK(camera.distance() == doctest::Approx(Camera::kMaxDistance));
}

TEST_CASE("aspect follows the viewport and guards zero height")
{
    Camera camera;
    camera.setViewport(800, 400);
    CHECK(camera.aspect() == doctest::Approx(2.0f));
    camera.setViewport(800, 0);
    CHECK(camera.aspect() == doctest::Approx(1.0f));
}

TEST_CASE("projection uses Vulkan conventions")
{
    Camera camera;
    camera.setViewport(800, 400);
    const glm::mat4 p = camera.projection();
    CHECK(p[1][1] < 0.0f);
    CHECK(p[0][0] == doctest::Approx(-p[1][1] / 2.0f));

    const glm::vec4 centre = camera.viewProjection() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    CHECK(centre.z / centre.w > 0.0f);
    CHECK(centre.z / centre.w < 1.0f);

    // World up maps to negative clip-space Y (top of the framebuffer in Vulkan).
    const glm::vec4 top = camera.viewProjection() * glm::vec4(0.0f, 0.5f, 0.0f, 1.0f);
    CHECK(top.y / top.w < 0.0f);
}
