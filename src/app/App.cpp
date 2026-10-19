#include "app/App.h"

#include "app/MainFrame.h"
#include "platform/embed.h"

wxIMPLEMENT_APP(App);

// The application object is constructed before the toolkit initializes, so
// environment overrides (Linux X11 backend) still take effect here.
App::App()
{
    platform::prepareEnvironment();
}

bool App::OnInit()
{
    if (!wxApp::OnInit()) {
        return false;
    }
    auto* frame = new MainFrame();
    frame->Show();
    return true;
}

int App::OnRun()
{
    const int code = wxApp::OnRun();
    return failed_ ? 1 : code;
}
