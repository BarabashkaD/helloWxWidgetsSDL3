#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "render/VulkanContext.h"

struct SDL_Window;

namespace render {

class CubeMesh;
class CubePipeline;
class Swapchain;

// Draws the cube into an SDL window with Vulkan. Throws std::runtime_error on failure.
class Renderer {
public:
    Renderer(SDL_Window* window, std::uint32_t width, std::uint32_t height, bool enableValidation);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Drawable size in physical pixels; a zero dimension pauses drawing.
    void resize(std::uint32_t width, std::uint32_t height);
    void draw(const glm::mat4& mvp);
    void waitIdle();

private:
    static constexpr std::uint32_t kFramesInFlight = 2;

    struct Frame {
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkSemaphore imageAcquired = VK_NULL_HANDLE;
        VkFence inFlight = VK_NULL_HANDLE;
    };

    void createFrameResources();
    void destroyFrameResources() noexcept;
    void createRenderFinishedSemaphores();
    void destroyRenderFinishedSemaphores() noexcept;
    // Returns false while the surface has zero size (e.g. minimized).
    bool recreateSwapchain();
    bool surfaceHasZeroExtent() const;
    void record(VkCommandBuffer commands, std::uint32_t imageIndex, const glm::mat4& mvp);

    VulkanContext context_;
    std::unique_ptr<Swapchain> swapchain_;
    std::unique_ptr<CubePipeline> pipeline_;
    std::unique_ptr<CubeMesh> mesh_;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::array<Frame, kFramesInFlight> frames_{};
    std::vector<VkSemaphore> renderFinished_; // one per swapchain image
    std::uint32_t frameIndex_ = 0;
    std::uint32_t width_;
    std::uint32_t height_;
    bool swapchainDirty_ = false;
};

} // namespace render
