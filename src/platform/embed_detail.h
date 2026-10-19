#pragma once

#include <SDL3/SDL_properties.h>

struct SDL_Window;

namespace platform::detail {

// Takes ownership of `props`: adds the Vulkan flag, creates the window,
// destroys the properties. Throws std::runtime_error on failure.
SDL_Window* createEmbeddedWindow(SDL_PropertiesID props);

// Creates an empty property set or throws.
SDL_PropertiesID createProperties();

} // namespace platform::detail
