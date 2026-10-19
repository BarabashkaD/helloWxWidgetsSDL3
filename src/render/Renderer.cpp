#include "render/Renderer.h"

#include <exception>

#include "render/CubeMesh.h"
#include "render/CubePipeline.h"
#include "render/Swapchain.h"
#include "render/VkCheck.h"

namespace render {

Renderer::Renderer(SDL_Window* window, std::uint32_t width, std::uint32_t height, bool enableValidation)
    : context_(window, enableValidation)
    , width_(width)
    , height_(height)
{
    try {
        swapchain_ = std::make_unique<Swapchain>(context_, width, height);
        pipeline_ = std::make_unique<CubePipeline>(context_.device(), swapchain_->renderPass());
        mesh_ = std::make_unique<CubeMesh>(context_.allocator());
        createFrameResources();
        createRenderFinishedSemaphores();
    } catch (...) {
        destroyRenderFinishedSemaphores();
        destroyFrameResources();
        throw;
    }
}

Renderer::~Renderer()
{
    vkDeviceWaitIdle(context_.device());
    destroyFrameResources();
    mesh_.reset();
    pipeline_.reset();
    swapchain_.reset();
    // Present waits on these; destroy them only after the swapchain is gone.
    destroyRenderFinishedSemaphores();
}

void Renderer::resize(std::uint32_t width, std::uint32_t height)
{
    if (width == width_ && height == height_) {
        return;
    }
    width_ = width;
    height_ = height;
    swapchainDirty_ = true;
}

void Renderer::waitIdle()
{
    VK_CHECK(vkDeviceWaitIdle(context_.device()));
}

void Renderer::draw(const glm::mat4& mvp)
{
    if (width_ == 0 || height_ == 0) {
        return;
    }
    if (swapchainDirty_ && !recreateSwapchain()) {
        return;
    }

    const VkDevice device = context_.device();
    Frame& frame = frames_[frameIndex_];
    VK_CHECK(vkWaitForFences(device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX));

    std::uint32_t imageIndex = 0;
    const VkResult acquired = vkAcquireNextImageKHR(device, swapchain_->handle(), UINT64_MAX, frame.imageAcquired,
                                                    VK_NULL_HANDLE, &imageIndex);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchainDirty_ = true;
        return;
    }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
        throwVkError(acquired, "vkAcquireNextImageKHR");
    }
    if (acquired == VK_SUBOPTIMAL_KHR) {
        swapchainDirty_ = true;
    }

    VK_CHECK(vkResetFences(device, 1, &frame.inFlight));
    VK_CHECK(vkResetCommandBuffer(frame.commands, 0));
    record(frame.commands, imageIndex, mvp);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &frame.imageAcquired;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &frame.commands;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinished_[imageIndex];
    VK_CHECK(vkQueueSubmit(context_.graphicsQueue(), 1, &submit, frame.inFlight));

    const VkSwapchainKHR swapchain = swapchain_->handle();
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinished_[imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain;
    present.pImageIndices = &imageIndex;
    const VkResult presented = vkQueuePresentKHR(context_.presentQueue(), &present);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
        swapchainDirty_ = true;
    } else if (presented != VK_SUCCESS) {
        throwVkError(presented, "vkQueuePresentKHR");
    }

    frameIndex_ = (frameIndex_ + 1) % kFramesInFlight;
}

void Renderer::record(VkCommandBuffer commands, std::uint32_t imageIndex, const glm::mat4& mvp)
{
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(commands, &begin));

    const VkExtent2D extent = swapchain_->extent();
    VkClearValue clears[2]{};
    clears[0].color = {{0.02f, 0.02f, 0.025f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = swapchain_->renderPass();
    pass.framebuffer = swapchain_->framebuffer(imageIndex);
    pass.renderArea = {{0, 0}, extent};
    pass.clearValueCount = 2;
    pass.pClearValues = clears;
    vkCmdBeginRenderPass(commands, &pass, VK_SUBPASS_CONTENTS_INLINE);

    pipeline_->bind(commands);
    const VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height),
                              0.0f, 1.0f};
    vkCmdSetViewport(commands, 0, 1, &viewport);
    const VkRect2D scissor{{0, 0}, extent};
    vkCmdSetScissor(commands, 0, 1, &scissor);
    mesh_->bind(commands);
    pipeline_->pushMvp(commands, mvp);
    vkCmdDrawIndexed(commands, mesh_->indexCount(), 1, 0, 0, 0);

    vkCmdEndRenderPass(commands);
    VK_CHECK(vkEndCommandBuffer(commands));
}

bool Renderer::surfaceHasZeroExtent() const
{
    VkSurfaceCapabilitiesKHR capabilities{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice(), context_.surface(), &capabilities));
    return capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0;
}

// width_/height_ only gate the zero-size case: vk-bootstrap takes the extent from
// the surface whenever the platform reports one.
bool Renderer::recreateSwapchain()
{
    if (surfaceHasZeroExtent()) {
        return false;
    }
    VK_CHECK(vkDeviceWaitIdle(context_.device()));
    try {
        swapchain_->recreate(width_, height_);
    } catch (const std::exception&) {
        // The window may have been minimized after the check above; retry later.
        if (surfaceHasZeroExtent()) {
            return false;
        }
        throw;
    }
    destroyRenderFinishedSemaphores();
    createRenderFinishedSemaphores();
    swapchainDirty_ = false;
    return true;
}

void Renderer::createFrameResources()
{
    const VkDevice device = context_.device();

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context_.graphicsQueueFamily();
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool_));

    std::array<VkCommandBuffer, kFramesInFlight> buffers{};
    VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = kFramesInFlight;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, buffers.data()));

    const VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (std::uint32_t i = 0; i < kFramesInFlight; ++i) {
        frames_[i].commands = buffers[i];
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames_[i].imageAcquired));
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frames_[i].inFlight));
    }
}

void Renderer::destroyFrameResources() noexcept
{
    const VkDevice device = context_.device();
    for (Frame& frame : frames_) {
        if (frame.imageAcquired != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frame.imageAcquired, nullptr);
        }
        if (frame.inFlight != VK_NULL_HANDLE) {
            vkDestroyFence(device, frame.inFlight, nullptr);
        }
        frame = Frame{};
    }
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
}

void Renderer::createRenderFinishedSemaphores()
{
    const VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    renderFinished_.assign(swapchain_->imageCount(), VK_NULL_HANDLE);
    for (VkSemaphore& semaphore : renderFinished_) {
        VK_CHECK(vkCreateSemaphore(context_.device(), &info, nullptr, &semaphore));
    }
}

void Renderer::destroyRenderFinishedSemaphores() noexcept
{
    for (VkSemaphore semaphore : renderFinished_) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(context_.device(), semaphore, nullptr);
        }
    }
    renderFinished_.clear();
}

} // namespace render
