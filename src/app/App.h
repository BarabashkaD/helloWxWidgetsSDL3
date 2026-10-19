#pragma once

#include <wx/app.h>

class App : public wxApp {
public:
    App();

    bool OnInit() override;
    int OnRun() override;

    // Makes the process exit with a non-zero code once the main loop ends.
    void markFailed() { failed_ = true; }

private:
    bool failed_ = false;
};

wxDECLARE_APP(App);
