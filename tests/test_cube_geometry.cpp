#include <cmath>
#include <set>
#include <tuple>

#include <doctest/doctest.h>

#include "scene/CubeGeometry.h"

using scene::cubeIndices;
using scene::cubeVertices;

TEST_CASE("indices reference existing vertices")
{
    for (auto index : cubeIndices()) {
        CHECK(index < cubeVertices().size());
    }
}

TEST_CASE("all vertices lie on the unit cube corners")
{
    for (const auto& v : cubeVertices()) {
        CHECK(std::abs(v.position.x) == doctest::Approx(0.5f));
        CHECK(std::abs(v.position.y) == doctest::Approx(0.5f));
        CHECK(std::abs(v.position.z) == doctest::Approx(0.5f));
    }
}

TEST_CASE("every triangle is counter-clockwise seen from outside")
{
    const auto& v = cubeVertices();
    const auto& idx = cubeIndices();
    for (std::size_t t = 0; t < idx.size(); t += 3) {
        const glm::vec3 a = v[idx[t]].position;
        const glm::vec3 b = v[idx[t + 1]].position;
        const glm::vec3 c = v[idx[t + 2]].position;
        const glm::vec3 normal = glm::cross(b - a, c - a);
        const glm::vec3 centroid = (a + b + c) / 3.0f;
        CHECK(glm::dot(normal, centroid) > 0.0f);
    }
}

TEST_CASE("each face has one colour and all six colours differ")
{
    const auto& v = cubeVertices();
    std::set<std::tuple<float, float, float>> colours;
    for (std::size_t face = 0; face < 6; ++face) {
        const glm::vec3 c = v[face * 4].color;
        for (std::size_t k = 1; k < 4; ++k) {
            CHECK(v[face * 4 + k].color == c);
        }
        colours.insert({c.r, c.g, c.b});
    }
    CHECK(colours.size() == 6);
}
