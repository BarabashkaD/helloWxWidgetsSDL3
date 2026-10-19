#pragma once

#include <wx/frame.h>

class wxButton;
class RenderPanel;

// Main window: render panel, bottom bar with the Start/Stop button, status bar.
class MainFrame : public wxFrame {
public:
    MainFrame();

private:
    void onToggleSpin(wxCommandEvent& event);
    void showFps(double fps);
    void showSpinState();
    void reportFatalError(const wxString& message);

    RenderPanel* renderPanel_ = nullptr;
    wxButton* toggleButton_ = nullptr;
};
