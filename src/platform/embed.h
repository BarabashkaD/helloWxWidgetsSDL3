#pragma once

struct SDL_Window;
class wxWindow;

namespace platform {

// Adjusts the process environment before any toolkit initializes.
// Linux forces the X11 backends (unless the user set them); elsewhere a no-op.
void prepareEnvironment();

// Wraps the native window of `window` in an SDL window usable for Vulkan.
// SDL video must be initialized. Throws std::runtime_error on failure.
SDL_Window* embedNativeWindow(wxWindow& window);

} // namespace platform
