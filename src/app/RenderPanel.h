#pragma once

#include <chrono>
#include <exception>
#include <functional>
#include <memory>

#include <wx/window.h>

#include "scene/Camera.h"
#include "scene/FpsCounter.h"
#include "scene/SpinController.h"

struct SDL_Window;

namespace render {
class Renderer;
}

// Native child window hosting the SDL3/Vulkan view. Translates wx input into
// scene updates and drives one frame per idle event.
class RenderPanel : public wxWindow {
public:
    explicit RenderPanel(wxWindow* parent);
    ~RenderPanel() override;

    void setSpinning(bool spinning) { spin_.setSpinning(spinning); }
    bool isSpinning() const { return spin_.isSpinning(); }

    void setFpsCallback(std::function<void(double)> callback) { onFps_ = std::move(callback); }
    void setErrorCallback(std::function<void(const wxString&)> callback) { onError_ = std::move(callback); }

private:
    void onIdle(wxIdleEvent& event);
    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);
    void onLeftDown(wxMouseEvent& event);
    void onLeftUp(wxMouseEvent& event);
    void onMotion(wxMouseEvent& event);
    void onWheel(wxMouseEvent& event);
    void onCaptureLost(wxMouseCaptureLostEvent& event);

    bool canRender() const;
    bool ensureRenderer();
    void renderFrame();
    void finishDrag();
    void shutdownRenderer() noexcept;
    void fail(const char* what, const std::exception& error);
    wxSize pixelSize() const;
    double now() const;

    std::function<void(double)> onFps_;
    std::function<void(const wxString&)> onError_;

    scene::SpinController spin_;
    scene::Camera camera_;
    scene::FpsCounter fps_;
    const std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    double lastFrameTime_ = -1.0;

    bool sdlStarted_ = false;
    SDL_Window* sdlWindow_ = nullptr;
    std::unique_ptr<render::Renderer> renderer_;
    bool failed_ = false;
    bool creating_ = false;
};
