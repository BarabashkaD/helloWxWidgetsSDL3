#include "platform/embed_detail.h"

#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>

namespace platform::detail {

SDL_PropertiesID createProperties()
{
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0) {
        throw std::runtime_error(std::string("SDL_CreateProperties failed: ") + SDL_GetError());
    }
    return props;
}

SDL_Window* createEmbeddedWindow(SDL_PropertiesID props)
{
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
    SDL_Window* window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!window) {
        throw std::runtime_error(std::string("Embedding the render window in SDL failed: ") + SDL_GetError());
    }
    return window;
}

} // namespace platform::detail
