#pragma once

#include <vulkan/vulkan.h>

namespace render {

const char* vkResultName(VkResult result);
[[noreturn]] void throwVkError(VkResult result, const char* call);

} // namespace render

#define VK_CHECK(call)                                          \
    do {                                                        \
        const VkResult vkCheckResult_ = (call);                 \
        if (vkCheckResult_ != VK_SUCCESS) {                     \
            ::render::throwVkError(vkCheckResult_, #call);      \
        }                                                       \
    } while (false)
