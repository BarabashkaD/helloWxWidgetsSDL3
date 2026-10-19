#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

namespace scene {

struct CubeVertex {
    glm::vec3 position;
    glm::vec3 color;
};

// Unit cube centred at the origin (half-extent 0.5), one solid colour per face,
// counter-clockwise winding when viewed from outside.
const std::array<CubeVertex, 24>& cubeVertices();
const std::array<std::uint16_t, 36>& cubeIndices();

} // namespace scene
