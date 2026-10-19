#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

struct SDL_Window;

namespace render {

// Instance, surface, device, queues and allocator for one SDL window.
class VulkanContext {
public:
    VulkanContext(SDL_Window* window, bool enableValidation);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VkDevice device() const { return device_.device; }
    VkPhysicalDevice physicalDevice() const { return device_.physical_device.physical_device; }
    VkSurfaceKHR surface() const { return surface_; }
    const vkb::Device& vkbDevice() const { return device_; }
    VkQueue graphicsQueue() const { return graphicsQueue_; }
    std::uint32_t graphicsQueueFamily() const { return graphicsQueueFamily_; }
    VkQueue presentQueue() const { return presentQueue_; }
    VmaAllocator allocator() const { return allocator_; }

private:
    void cleanup() noexcept;

    vkb::Instance instance_;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    vkb::Device device_;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    std::uint32_t graphicsQueueFamily_ = 0;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
};

} // namespace render
