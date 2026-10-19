#include "platform/embed.h"

#include <cstdlib>
#include <stdexcept>

#include <wx/window.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_video.h>

#include "platform/embed_detail.h"

namespace platform {

void prepareEnvironment()
{
    // Respect values the user already set.
    setenv("GDK_BACKEND", "x11", 0);
    setenv("SDL_VIDEO_DRIVER", "x11", 0);
}

SDL_Window* embedNativeWindow(wxWindow& window)
{
    GtkWidget* widget = static_cast<GtkWidget*>(window.GetHandle());
    gtk_widget_realize(widget);
    GdkWindow* gdkWindow = window.GTKGetDrawingWindow();
    if (!gdkWindow) {
        gdkWindow = gtk_widget_get_window(widget);
    }
    if (!gdkWindow || !GDK_IS_X11_WINDOW(gdkWindow)) {
        throw std::runtime_error("The render panel has no X11 window; run under X11 or XWayland");
    }
    gdk_window_ensure_native(gdkWindow);
    // SDL uses its own X connection; make sure the server has created the window first.
    gdk_display_sync(gdk_window_get_display(gdkWindow));
    const auto xid = gdk_x11_window_get_xid(gdkWindow);

    // wxWidgets keeps handling mouse input; SDL must not select input on this window.
    SDL_SetHint(SDL_HINT_VIDEO_X11_EXTERNAL_WINDOW_INPUT, "0");

    const SDL_PropertiesID props = detail::createProperties();
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER, static_cast<Sint64>(xid));
    return detail::createEmbeddedWindow(props);
}

} // namespace platform
