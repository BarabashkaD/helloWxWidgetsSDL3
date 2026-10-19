#include <cmath>

#include <doctest/doctest.h>
#include <glm/gtc/quaternion.hpp>

#include "scene/SpinController.h"

using scene::SpinController;

namespace {

bool near(glm::vec3 a, glm::vec3 b, float eps = 1e-4f)
{
    return glm::length(a - b) < eps;
}

glm::vec3 front(const SpinController& spin)
{
    return spin.orientation() * glm::vec3(0.0f, 0.0f, 1.0f);
}

// Five samples 20 ms apart, each moving by `step` pixels, released at t = 0.1 s.
void flick(SpinController& spin, glm::vec2 step)
{
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    for (int i = 1; i <= 5; ++i) {
        spin.dragTo(step * static_cast<float>(i), 0.02 * i);
    }
    spin.endDrag(0.1);
}

const glm::vec3 kDefaultAxis = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));

} // namespace

TEST_CASE("starts spinning about normalize(1,1,0) at 90 degrees per second")
{
    SpinController spin;
    CHECK(spin.isSpinning());
    CHECK_FALSE(spin.isDragging());
    CHECK(near(spin.axis(), kDefaultAxis));
    CHECK(spin.speed() == doctest::Approx(1.5707963f));
}

TEST_CASE("update while spinning rotates by speed * dt about the axis")
{
    SpinController spin;
    spin.update(0.5f);
    const glm::quat expected = glm::angleAxis(spin.speed() * 0.5f, kDefaultAxis);
    CHECK(near(front(spin), expected * glm::vec3(0.0f, 0.0f, 1.0f)));
}

TEST_CASE("update while stopped leaves the orientation unchanged")
{
    SpinController spin;
    spin.setSpinning(false);
    spin.update(1.0f);
    CHECK(near(front(spin), {0.0f, 0.0f, 1.0f}));
}

TEST_CASE("dragging right rotates about +Y by 0.01 rad per pixel")
{
    SpinController spin;
    spin.setSpinning(false);
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    spin.dragTo({100.0f, 0.0f}, 0.5);
    CHECK(near(front(spin), {std::sin(1.0f), 0.0f, std::cos(1.0f)}));
}

TEST_CASE("dragging down moves the front face down")
{
    SpinController spin;
    spin.setSpinning(false);
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    spin.dragTo({0.0f, 100.0f}, 0.5);
    CHECK(near(front(spin), {0.0f, -std::sin(1.0f), std::cos(1.0f)}));
}

TEST_CASE("automatic spin pauses while dragging but stays enabled")
{
    SpinController spin;
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    spin.update(1.0f);
    CHECK(near(front(spin), {0.0f, 0.0f, 1.0f}));
    CHECK(spin.isSpinning());
    CHECK(spin.isDragging());
}

TEST_CASE("fast horizontal flick sets the spin axis to +Y")
{
    SpinController spin;
    flick(spin, {10.0f, 0.0f});
    CHECK_FALSE(spin.isDragging());
    CHECK(near(spin.axis(), {0.0f, 1.0f, 0.0f}));
}

TEST_CASE("fast downward flick sets the spin axis to +X")
{
    SpinController spin;
    flick(spin, {0.0f, 10.0f});
    CHECK(near(spin.axis(), {1.0f, 0.0f, 0.0f}));
}

TEST_CASE("flick to the left reverses the direction")
{
    SpinController spin;
    flick(spin, {-10.0f, 0.0f});
    CHECK(near(spin.axis(), {0.0f, -1.0f, 0.0f}));
}

TEST_CASE("slow release keeps the previous axis")
{
    SpinController spin;
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    spin.dragTo({2.0f, 0.0f}, 0.05);
    spin.endDrag(0.1);
    CHECK(near(spin.axis(), kDefaultAxis));
}

TEST_CASE("holding still before release keeps the previous axis")
{
    SpinController spin;
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    spin.dragTo({50.0f, 0.0f}, 0.05);
    spin.endDrag(1.0);
    CHECK(near(spin.axis(), kDefaultAxis));
}

TEST_CASE("click without movement keeps the previous axis")
{
    SpinController spin;
    spin.beginDrag({5.0f, 5.0f}, 0.0);
    spin.endDrag(0.05);
    CHECK(near(spin.axis(), kDefaultAxis));
}

TEST_CASE("flick keeps the speed and the stopped state")
{
    SpinController spin;
    spin.setSpinning(false);
    flick(spin, {10.0f, 0.0f});
    CHECK_FALSE(spin.isSpinning());
    CHECK(spin.speed() == doctest::Approx(SpinController::kDefaultSpeed));
    CHECK(near(spin.axis(), {0.0f, 1.0f, 0.0f}));
}
