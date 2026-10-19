#include "render/CubeMesh.h"

#include <cstddef>
#include <cstring>

#include "render/VkCheck.h"
#include "scene/CubeGeometry.h"

namespace render {

CubeMesh::CubeMesh(VmaAllocator allocator)
    : allocator_(allocator)
{
    const auto& vertices = scene::cubeVertices();
    const auto& indices = scene::cubeIndices();
    try {
        vertices_ = upload(vertices.data(), sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        indices_ = upload(indices.data(), sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    } catch (...) {
        destroy(vertices_);
        destroy(indices_);
        throw;
    }
}

CubeMesh::~CubeMesh()
{
    destroy(indices_);
    destroy(vertices_);
}

void CubeMesh::bind(VkCommandBuffer commands) const
{
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commands, 0, 1, &vertices_.buffer, &offset);
    vkCmdBindIndexBuffer(commands, indices_.buffer, 0, VK_INDEX_TYPE_UINT16);
}

std::uint32_t CubeMesh::indexCount() const
{
    return static_cast<std::uint32_t>(scene::cubeIndices().size());
}

VkVertexInputBindingDescription CubeMesh::bindingDescription()
{
    return {0, sizeof(scene::CubeVertex), VK_VERTEX_INPUT_RATE_VERTEX};
}

std::array<VkVertexInputAttributeDescription, 2> CubeMesh::attributeDescriptions()
{
    return {{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(scene::CubeVertex, position))},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(scene::CubeVertex, color))},
    }};
}

CubeMesh::Buffer CubeMesh::upload(const void* data, VkDeviceSize size, VkBufferUsageFlags usage)
{
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    Buffer result;
    VmaAllocationInfo info{};
    VK_CHECK(vmaCreateBuffer(allocator_, &bufferInfo, &allocationInfo, &result.buffer, &result.allocation, &info));
    std::memcpy(info.pMappedData, data, static_cast<std::size_t>(size));
    const VkResult flushed = vmaFlushAllocation(allocator_, result.allocation, 0, VK_WHOLE_SIZE);
    if (flushed != VK_SUCCESS) {
        destroy(result);
        throwVkError(flushed, "vmaFlushAllocation");
    }
    return result;
}

void CubeMesh::destroy(Buffer& buffer) noexcept
{
    if (buffer.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
        buffer = Buffer{};
    }
}

} // namespace render
