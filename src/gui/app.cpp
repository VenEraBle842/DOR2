#include "gui/main_frame.hpp"

#include <wx/wx.h>

class Dor2App : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit()) return false;
        SetAppName("DOR2");
        auto* frame = new MainFrame;
        frame->Show();
        return true;
    }

    bool OnExceptionInMainLoop() override {
        try { throw; }
        catch (const std::exception& error) {
            wxMessageBox(wxString::FromUTF8(formatError(makeError(AppErrorCode::Unexpected,
                {.detail = error.what()}))), "Operation failed", wxOK | wxICON_ERROR);
        } catch (...) {
            wxMessageBox(wxString::FromUTF8(formatError(makeError(AppErrorCode::Unexpected))),
                         "Operation failed", wxOK | wxICON_ERROR);
        }
        return true;
    }
};

wxIMPLEMENT_APP(Dor2App);
