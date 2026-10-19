#include "render/Swapchain.h"

#include <stdexcept>

#include "render/VkCheck.h"
#include "render/VulkanContext.h"

namespace render {

namespace {

VkFormat chooseDepthFormat(VkPhysicalDevice gpu)
{
    for (VkFormat format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
                            VK_FORMAT_D16_UNORM}) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(gpu, format, &properties);
        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    throw std::runtime_error("No supported depth buffer format");
}

VkImageAspectFlags depthAspect(VkFormat format)
{
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    return aspect;
}

} // namespace

Swapchain::Swapchain(VulkanContext& context, std::uint32_t width, std::uint32_t height)
    : context_(context)
{
    try {
        depthFormat_ = chooseDepthFormat(context_.physicalDevice());
        createSwapchain(width, height);
        createRenderPass();
        createSizedResources();
    } catch (...) {
        destroyAll();
        throw;
    }
}

Swapchain::~Swapchain()
{
    destroyAll();
}

void Swapchain::recreate(std::uint32_t width, std::uint32_t height)
{
    const VkFormat previousFormat = swapchain_.image_format;
    destroySizedResources();
    createSwapchain(width, height);
    // The render pass and the pipeline were built for the original format.
    if (swapchain_.image_format != previousFormat) {
        throw std::runtime_error("Swapchain image format changed during recreation");
    }
    createSizedResources();
}

void Swapchain::createSwapchain(std::uint32_t width, std::uint32_t height)
{
    vkb::SwapchainBuilder builder{context_.vkbDevice()};
    auto result = builder.set_old_swapchain(swapchain_.swapchain)
                      .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                      .add_fallback_format({VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                      .set_desired_extent(width, height)
                      .build();
    if (!result) {
        throw std::runtime_error("Swapchain creation failed: " + result.error().message());
    }
    vkb::Swapchain old = swapchain_;
    swapchain_ = result.value();
    if (old.swapchain != VK_NULL_HANDLE) {
        vkb::destroy_swapchain(old);
    }
}

void Swapchain::createRenderPass()
{
    VkAttachmentDescription attachments[2]{};
    attachments[0].format = swapchain_.image_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format = depthFormat_;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    const VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // Orders this frame's color/depth writes after the previous frame's (the depth
    // image is shared by all frames in flight) and after image acquisition.
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    const VkPipelineStageFlags stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcStageMask = stages;
    dependency.srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = stages;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = 2;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;
    VK_CHECK(vkCreateRenderPass(context_.device(), &info, nullptr, &renderPass_));
}

void Swapchain::createSizedResources()
{
    auto views = swapchain_.get_image_views();
    if (!views) {
        throw std::runtime_error("Swapchain image views failed: " + views.error().message());
    }
    imageViews_ = views.value();

    const VkExtent2D size = swapchain_.extent;

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = depthFormat_;
    imageInfo.extent = {size.width, size.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    VK_CHECK(vmaCreateImage(context_.allocator(), &imageInfo, &allocationInfo, &depthImage_, &depthAllocation_,
                            nullptr));

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat_;
    viewInfo.subresourceRange = {depthAspect(depthFormat_), 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(context_.device(), &viewInfo, nullptr, &depthView_));

    framebuffers_.reserve(imageViews_.size());
    for (VkImageView colorView : imageViews_) {
        const VkImageView attachments[2] = {colorView, depthView_};
        VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = renderPass_;
        info.attachmentCount = 2;
        info.pAttachments = attachments;
        info.width = size.width;
        info.height = size.height;
        info.layers = 1;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VK_CHECK(vkCreateFramebuffer(context_.device(), &info, nullptr, &framebuffer));
        framebuffers_.push_back(framebuffer);
    }
}

void Swapchain::destroySizedResources() noexcept
{
    const VkDevice device = context_.device();
    for (VkFramebuffer framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    framebuffers_.clear();
    if (depthView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, depthView_, nullptr);
        depthView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vmaDestroyImage(context_.allocator(), depthImage_, depthAllocation_);
        depthImage_ = VK_NULL_HANDLE;
        depthAllocation_ = VK_NULL_HANDLE;
    }
    if (!imageViews_.empty()) {
        swapchain_.destroy_image_views(imageViews_);
        imageViews_.clear();
    }
}

void Swapchain::destroyAll() noexcept
{
    destroySizedResources();
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context_.device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    if (swapchain_.swapchain != VK_NULL_HANDLE) {
        vkb::destroy_swapchain(swapchain_);
        swapchain_ = vkb::Swapchain{};
    }
}

} // namespace render
