#include "render/VulkanContext.h"

#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_vulkan.h>

#include "render/Log.h"
#include "render/VkCheck.h"

namespace render {

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        log((std::string("[vulkan] ") + data->pMessage).c_str());
    }
    return VK_FALSE;
}

[[noreturn]] void fail(const std::string& what, const std::string& reason)
{
    throw std::runtime_error(what + ": " + reason);
}

} // namespace

VulkanContext::VulkanContext(SDL_Window* window, bool enableValidation)
{
    try {
        Uint32 extensionCount = 0;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (!extensions) {
            fail("SDL_Vulkan_GetInstanceExtensions failed", SDL_GetError());
        }

        vkb::InstanceBuilder builder;
        builder.set_app_name("cubeapp").require_api_version(1, 1, 0).enable_extensions(extensionCount, extensions);
        if (enableValidation) {
            auto system = vkb::SystemInfo::get_system_info();
            if (system && system->validation_layers_available) {
                builder.request_validation_layers(true).set_debug_callback(debugCallback);
            } else {
                log("Vulkan validation layer not available; continuing without it");
            }
        }
        auto instance = builder.build();
        if (!instance) {
            fail("Vulkan instance creation failed", instance.error().message());
        }
        instance_ = instance.value();

        if (!SDL_Vulkan_CreateSurface(window, instance_.instance, nullptr, &surface_)) {
            fail("Vulkan surface creation failed", SDL_GetError());
        }

        vkb::PhysicalDeviceSelector selector{instance_};
        auto physical = selector.set_surface(surface_)
                            .set_minimum_version(1, 1)
                            .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                            .select();
        if (!physical) {
            fail("No suitable GPU", physical.error().message());
        }

        auto device = vkb::DeviceBuilder{physical.value()}.build();
        if (!device) {
            fail("Vulkan device creation failed", device.error().message());
        }
        device_ = device.value();

        auto graphicsQueue = device_.get_queue(vkb::QueueType::graphics);
        auto graphicsFamily = device_.get_queue_index(vkb::QueueType::graphics);
        auto presentQueue = device_.get_queue(vkb::QueueType::present);
        if (!graphicsQueue || !graphicsFamily || !presentQueue) {
            fail("Vulkan queue lookup failed", "no graphics or present queue");
        }
        graphicsQueue_ = graphicsQueue.value();
        graphicsQueueFamily_ = graphicsFamily.value();
        presentQueue_ = presentQueue.value();

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
        allocatorInfo.instance = instance_.instance;
        allocatorInfo.physicalDevice = physicalDevice();
        allocatorInfo.device = device_.device;
        VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator_));
    } catch (...) {
        cleanup();
        throw;
    }
}

VulkanContext::~VulkanContext()
{
    cleanup();
}

void VulkanContext::cleanup() noexcept
{
    if (allocator_ != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
    }
    if (device_.device != VK_NULL_HANDLE) {
        vkb::destroy_device(device_);
        device_ = vkb::Device{};
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkb::destroy_surface(instance_, surface_);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_.instance != VK_NULL_HANDLE) {
        vkb::destroy_instance(instance_);
        instance_ = vkb::Instance{};
    }
}

} // namespace render
