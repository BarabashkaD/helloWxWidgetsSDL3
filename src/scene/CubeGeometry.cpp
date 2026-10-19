#include "scene/CubeGeometry.h"

namespace scene {

namespace {

constexpr glm::vec3 kRed{1.0f, 0.0f, 0.0f};
constexpr glm::vec3 kGreen{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 kBlue{0.0f, 0.0f, 1.0f};
constexpr glm::vec3 kYellow{1.0f, 1.0f, 0.0f};
constexpr glm::vec3 kCyan{0.0f, 1.0f, 1.0f};
constexpr glm::vec3 kMagenta{1.0f, 0.0f, 1.0f};

constexpr float h = 0.5f;

// Per face: bottom-left, bottom-right, top-right, top-left as seen from outside.
const std::array<CubeVertex, 24> kVertices{{
    // +X
    {{h, -h, h}, kRed}, {{h, -h, -h}, kRed}, {{h, h, -h}, kRed}, {{h, h, h}, kRed},
    // -X
    {{-h, -h, -h}, kGreen}, {{-h, -h, h}, kGreen}, {{-h, h, h}, kGreen}, {{-h, h, -h}, kGreen},
    // +Y
    {{-h, h, h}, kBlue}, {{h, h, h}, kBlue}, {{h, h, -h}, kBlue}, {{-h, h, -h}, kBlue},
    // -Y
    {{-h, -h, -h}, kYellow}, {{h, -h, -h}, kYellow}, {{h, -h, h}, kYellow}, {{-h, -h, h}, kYellow},
    // +Z
    {{-h, -h, h}, kCyan}, {{h, -h, h}, kCyan}, {{h, h, h}, kCyan}, {{-h, h, h}, kCyan},
    // -Z
    {{h, -h, -h}, kMagenta}, {{-h, -h, -h}, kMagenta}, {{-h, h, -h}, kMagenta}, {{h, h, -h}, kMagenta},
}};

std::array<std::uint16_t, 36> makeIndices()
{
    std::array<std::uint16_t, 36> indices{};
    for (std::uint16_t face = 0; face < 6; ++face) {
        const auto base = static_cast<std::uint16_t>(face * 4);
        const std::uint16_t quad[6] = {0, 1, 2, 2, 3, 0};
        for (std::size_t k = 0; k < 6; ++k) {
            indices[face * 6 + k] = static_cast<std::uint16_t>(base + quad[k]);
        }
    }
    return indices;
}

const std::array<std::uint16_t, 36> kIndices = makeIndices();

} // namespace

const std::array<CubeVertex, 24>& cubeVertices()
{
    return kVertices;
}

const std::array<std::uint16_t, 36>& cubeIndices()
{
    return kIndices;
}

} // namespace scene
