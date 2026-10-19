#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace render {

// Graphics pipeline for the cube: MVP push constant, depth test, back-face culling.
class CubePipeline {
public:
    CubePipeline(VkDevice device, VkRenderPass renderPass);
    ~CubePipeline();

    CubePipeline(const CubePipeline&) = delete;
    CubePipeline& operator=(const CubePipeline&) = delete;

    void bind(VkCommandBuffer commands) const;
    void pushMvp(VkCommandBuffer commands, const glm::mat4& mvp) const;

private:
    VkDevice device_;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace render
