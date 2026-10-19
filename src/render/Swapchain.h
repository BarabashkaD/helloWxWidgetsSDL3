#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

namespace render {

class VulkanContext;

// Swapchain plus everything sized to it: image views, depth buffer, framebuffers.
// The render pass is created once and survives recreation.
class Swapchain {
public:
    Swapchain(VulkanContext& context, std::uint32_t width, std::uint32_t height);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // The caller must make sure the device is idle.
    void recreate(std::uint32_t width, std::uint32_t height);

    VkSwapchainKHR handle() const { return swapchain_.swapchain; }
    VkExtent2D extent() const { return swapchain_.extent; }
    std::uint32_t imageCount() const { return static_cast<std::uint32_t>(framebuffers_.size()); }
    VkRenderPass renderPass() const { return renderPass_; }
    VkFramebuffer framebuffer(std::uint32_t index) const { return framebuffers_[index]; }

private:
    void createSwapchain(std::uint32_t width, std::uint32_t height);
    void createRenderPass();
    void createSizedResources();
    void destroySizedResources() noexcept;
    void destroyAll() noexcept;

    VulkanContext& context_;
    vkb::Swapchain swapchain_;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkImageView> imageViews_;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VmaAllocation depthAllocation_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
};

} // namespace render
