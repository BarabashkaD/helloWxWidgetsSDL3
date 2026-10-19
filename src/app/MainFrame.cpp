#include "app/MainFrame.h"

#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>

#include "app/App.h"
#include "app/RenderPanel.h"

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "wxWidgets + SDL3 + Vulkan Cube")
{
    renderPanel_ = new RenderPanel(this);

    auto* bar = new wxPanel(this);
    toggleButton_ = new wxButton(bar, wxID_ANY, "Start");
    // Fix the width now, while the best size is still computed for "Start", so GTK does not clip the label.
    toggleButton_->SetMinSize(toggleButton_->GetBestSize());
    auto* barSizer = new wxBoxSizer(wxHORIZONTAL);
    barSizer->AddStretchSpacer();
    barSizer->Add(toggleButton_, wxSizerFlags().Border(wxALL, FromDIP(8)));
    barSizer->AddStretchSpacer();
    bar->SetSizer(barSizer);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(renderPanel_, wxSizerFlags(1).Expand());
    sizer->Add(bar, wxSizerFlags().Expand());
    SetSizer(sizer);

    CreateStatusBar(2);
    SetStatusText("FPS: --", 0);
    showSpinState();

    toggleButton_->Bind(wxEVT_BUTTON, &MainFrame::onToggleSpin, this);
    renderPanel_->setFpsCallback([this](double fps) { showFps(fps); });
    renderPanel_->setErrorCallback([this](const wxString& message) { reportFatalError(message); });

    SetMinSize(FromDIP(wxSize(320, 240)));
    SetClientSize(FromDIP(wxSize(960, 640)));
    Centre();
}

void MainFrame::onToggleSpin(wxCommandEvent&)
{
    renderPanel_->setSpinning(!renderPanel_->isSpinning());
    showSpinState();
}

void MainFrame::showSpinState()
{
    const bool spinning = renderPanel_->isSpinning();
    toggleButton_->SetLabel(spinning ? "Stop" : "Start");
    SetStatusText(spinning ? "Spinning" : "Stopped", 1);
}

void MainFrame::showFps(double fps)
{
    SetStatusText(wxString::Format("FPS: %.1f", fps), 0);
}

void MainFrame::reportFatalError(const wxString& message)
{
    CallAfter([this, message] {
        wxMessageBox(message, "Rendering failed", wxOK | wxICON_ERROR, this);
        wxGetApp().markFailed();
        Close(true);
    });
}
