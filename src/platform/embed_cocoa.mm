#include "platform/embed.h"

#include <wx/window.h>

#import <Cocoa/Cocoa.h>

#include <SDL3/SDL_video.h>

#include "platform/embed_detail.h"

namespace platform {

void prepareEnvironment()
{
}

SDL_Window* embedNativeWindow(wxWindow& window)
{
    NSView* view = window.GetHandle();
    const SDL_PropertiesID props = detail::createProperties();
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, (__bridge void*)[view window]);
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER, (__bridge void*)view);
    return detail::createEmbeddedWindow(props);
}

} // namespace platform
