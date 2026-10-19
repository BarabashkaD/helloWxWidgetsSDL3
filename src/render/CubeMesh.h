#pragma once

#include <array>
#include <cstdint>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace render {

// Vertex and index buffers for scene::cubeVertices() / scene::cubeIndices().
class CubeMesh {
public:
    explicit CubeMesh(VmaAllocator allocator);
    ~CubeMesh();

    CubeMesh(const CubeMesh&) = delete;
    CubeMesh& operator=(const CubeMesh&) = delete;

    void bind(VkCommandBuffer commands) const;
    std::uint32_t indexCount() const;

    static VkVertexInputBindingDescription bindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions();

private:
    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    Buffer upload(const void* data, VkDeviceSize size, VkBufferUsageFlags usage);
    void destroy(Buffer& buffer) noexcept;

    VmaAllocator allocator_;
    Buffer vertices_;
    Buffer indices_;
};

} // namespace render
