#include "app/RenderPanel.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <wx/dcclient.h>
#include <wx/toplevel.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include <glm/gtc/quaternion.hpp>

#include "platform/embed.h"
#include "render/Renderer.h"

namespace {

#ifdef NDEBUG
constexpr bool kEnableValidation = false;
#else
constexpr bool kEnableValidation = true;
#endif

constexpr float kMaxFrameTime = 0.1f; // seconds; avoids a jump after stalls

glm::vec2 toVec(const wxPoint& point)
{
    return {static_cast<float>(point.x), static_cast<float>(point.y)};
}

} // namespace

RenderPanel::RenderPanel(wxWindow* parent)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
#ifdef __WXGTK__
    // GTK's double buffering would paint over the Vulkan output (as for wxGLCanvas).
    SetDoubleBuffered(false);
#endif

    Bind(wxEVT_IDLE, &RenderPanel::onIdle, this);
    Bind(wxEVT_PAINT, &RenderPanel::onPaint, this);
    Bind(wxEVT_SIZE, &RenderPanel::onSize, this);
    Bind(wxEVT_LEFT_DOWN, &RenderPanel::onLeftDown, this);
    Bind(wxEVT_LEFT_DCLICK, &RenderPanel::onLeftDown, this); // fast re-grab arrives as a double click
    Bind(wxEVT_LEFT_UP, &RenderPanel::onLeftUp, this);
    Bind(wxEVT_MOTION, &RenderPanel::onMotion, this);
    Bind(wxEVT_MOUSEWHEEL, &RenderPanel::onWheel, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &RenderPanel::onCaptureLost, this);
}

RenderPanel::~RenderPanel()
{
    shutdownRenderer();
}

void RenderPanel::onIdle(wxIdleEvent& event)
{
    renderFrame();
    if (canRender()) {
        event.RequestMore();
    }
}

void RenderPanel::onPaint(wxPaintEvent&)
{
    wxPaintDC dc(this); // validates the region; all drawing is done by Vulkan
    renderFrame();
}

void RenderPanel::onSize(wxSizeEvent& event)
{
    event.Skip();
    const wxSize size = pixelSize();
    camera_.setViewport(size.x, size.y);
    if (renderer_) {
        renderer_->resize(static_cast<std::uint32_t>(std::max(size.x, 0)),
                          static_cast<std::uint32_t>(std::max(size.y, 0)));
        renderFrame(); // keeps the view live during modal resize loops
    }
}

void RenderPanel::onLeftDown(wxMouseEvent& event)
{
    SetFocus();
    if (!HasCapture()) {
        CaptureMouse();
    }
    spin_.beginDrag(toVec(event.GetPosition()), now());
}

void RenderPanel::onLeftUp(wxMouseEvent&)
{
    finishDrag();
}

void RenderPanel::onMotion(wxMouseEvent& event)
{
    if (!spin_.isDragging()) {
        return;
    }
    if (!event.LeftIsDown()) {
        finishDrag();
        return;
    }
    spin_.dragTo(toVec(event.GetPosition()), now());
}

void RenderPanel::onWheel(wxMouseEvent& event)
{
    const int delta = event.GetWheelDelta();
    if (delta == 0 || event.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
        return;
    }
    camera_.zoom(static_cast<float>(event.GetWheelRotation()) / static_cast<float>(delta));
}

void RenderPanel::onCaptureLost(wxMouseCaptureLostEvent&)
{
    if (spin_.isDragging()) {
        spin_.endDrag(now());
    }
}

void RenderPanel::finishDrag()
{
    if (spin_.isDragging()) {
        spin_.endDrag(now());
    }
    if (HasCapture()) {
        ReleaseMouse();
    }
}

bool RenderPanel::canRender() const
{
    if (failed_ || !IsShownOnScreen()) {
        return false;
    }
    const auto* topLevel = wxDynamicCast(wxGetTopLevelParent(const_cast<RenderPanel*>(this)), wxTopLevelWindow);
    if (topLevel && topLevel->IsIconized()) {
        return false;
    }
    const wxSize size = pixelSize();
    return size.x > 0 && size.y > 0;
}

bool RenderPanel::ensureRenderer()
{
    if (renderer_) {
        return true;
    }
    if (creating_ || !canRender()) {
        return false;
    }
    creating_ = true;
    struct Reset {
        bool& flag;
        ~Reset() { flag = false; }
    } resetCreating{creating_};
    const wxSize size = pixelSize();
    try {
        if (!sdlStarted_) {
            // wxWidgets owns mouse capture, and SDL must not block the screensaver.
            SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
            SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
            if (!SDL_Init(SDL_INIT_VIDEO)) {
                throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
            }
            sdlStarted_ = true;
        }
        sdlWindow_ = platform::embedNativeWindow(*this);
        renderer_ = std::make_unique<render::Renderer>(sdlWindow_, static_cast<std::uint32_t>(size.x),
                                                       static_cast<std::uint32_t>(size.y), kEnableValidation);
        camera_.setViewport(size.x, size.y);
        return true;
    } catch (const std::exception& error) {
        fail("Vulkan initialisation failed", error);
        return false;
    }
}

void RenderPanel::renderFrame()
{
    if (!canRender() || !ensureRenderer()) {
        return;
    }
    const double time = now();
    const float dt = lastFrameTime_ < 0.0 ? 0.0f : static_cast<float>(time - lastFrameTime_);
    lastFrameTime_ = time;
    spin_.update(std::min(dt, kMaxFrameTime));

    try {
        renderer_->draw(camera_.viewProjection() * glm::mat4_cast(spin_.orientation()));
    } catch (const std::exception& error) {
        fail("Rendering failed", error);
        return;
    }

    // SDL queues events for the wrapped window that this app never reads.
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    if (fps_.frame(time) && onFps_) {
        onFps_(fps_.fps());
    }
}

void RenderPanel::shutdownRenderer() noexcept
{
    renderer_.reset(); // waits for the GPU, then destroys Vulkan objects
    if (sdlWindow_) {
        SDL_DestroyWindow(sdlWindow_);
        sdlWindow_ = nullptr;
    }
    if (sdlStarted_) {
        SDL_Quit();
        sdlStarted_ = false;
    }
}

void RenderPanel::fail(const char* what, const std::exception& error)
{
    failed_ = true;
    shutdownRenderer();
    if (onError_) {
        onError_(wxString::Format("%s:\n%s", what, error.what()));
    }
}

wxSize RenderPanel::pixelSize() const
{
    return ToPhys(GetClientSize());
}

double RenderPanel::now() const
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
}
