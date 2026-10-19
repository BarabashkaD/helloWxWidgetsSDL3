#include "platform/embed.h"

#include <wx/window.h>

#include <SDL3/SDL_video.h>

#include "platform/embed_detail.h"

namespace platform {

void prepareEnvironment()
{
}

SDL_Window* embedNativeWindow(wxWindow& window)
{
    const SDL_PropertiesID props = detail::createProperties();
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, window.GetHandle());
    return detail::createEmbeddedWindow(props);
}

} // namespace platform
