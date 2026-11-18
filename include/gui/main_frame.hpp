#pragma once

#include "application/student_application.hpp"
#include "containers/dynamic_array.hpp"

#include <wx/frame.h>
#include <wx/listctrl.h>

#include <functional>
#include <optional>
#include <thread>

class wxChoice;
class wxNotebook;
class wxPanel;
class wxStaticText;
class wxTextCtrl;
class wxBoxSizer;

class StudentTable : public wxListCtrl {
    DynamicArray<Student> rows_;
    wxString OnGetItemText(long item, long column) const override;
public:
    explicit StudentTable(wxWindow* parent, const wxString& name);
    void setRows(DynamicArray<Student> rows);
};

class MainFrame : public wxFrame {

    struct Snapshot {
        DynamicArray<Student> students;
        DynamicArray<BenchmarkResult> benchmark;
        std::optional<StudentIndexStatistics> statistics;
        std::string indexState;
    };
    struct TaskOutcome {
        std::optional<AppError> error;
        std::optional<Snapshot> snapshot;
        std::optional<DynamicArray<Student>> matches;
    };
    enum class DataEffect { None, Changed, Loaded, Saved, Search };
    using Action = std::function<std::expected<void, AppError>(StudentApplication&, TaskOutcome&)>;

    StudentApplication application_;
    std::jthread worker_;
    bool busy_{false};
    bool closeAfterTask_{false};
    bool dirty_{false};
    bool suppressErrorDialogs_{false};
    std::optional<AppError> lastError_;
    Snapshot snapshot_;

    wxNotebook* notebook_{};
    StudentTable* students_{};
    StudentTable* matches_{};
    wxStaticText* recordCount_{};
    wxStaticText* matchCount_{};
    wxStaticText* indexState_{};
    wxListCtrl* statistics_{};
    wxListCtrl* benchmark_{};
    wxTextCtrl* datasetSize_{};
    wxTextCtrl* queryCount_{};
    wxTextCtrl* benchmarkSeed_{};
    wxChoice* searchMode_{};
    wxChoice* searchAttribute_{};
    wxChoice* searchStrategy_{};
    wxStaticText* searchHint_{};
    wxStaticText* lowerLabel_{};
    wxStaticText* lowerValueLabel_{};
    wxStaticText* upperValueLabel_{};
    wxTextCtrl* lowerValue_{};
    wxTextCtrl* lowerFirstName_{};
    wxTextCtrl* lowerYear_{};
    wxTextCtrl* upperValue_{};
    wxTextCtrl* upperFirstName_{};
    wxTextCtrl* upperYear_{};
    wxBoxSizer* lowerFields_{};
    wxBoxSizer* upperFields_{};
    wxPanel* searchPanel_{};

    wxPanel* createStudentsPage();
    wxPanel* createSearchPage();
    wxPanel* createIndexesPage();
    wxPanel* createBenchmarkPage();
    void configureSearch(bool resetChoices);
    void search();
    void addStudent();
    void generateStudents();
    void loadStudents();
    void saveStudents();
    void runBenchmark();
    void exportBenchmark();
    void buildIndexes();
    void showError(const AppError& error);
    void setBusy(bool busy, const std::string& message);
    void refreshSnapshot();
    void runTask(const std::string& label, Action action, DataEffect effect = DataEffect::None);
    bool confirmReplace();
    void onClose(wxCloseEvent& event);
    static Snapshot takeSnapshot(const StudentApplication& app);

public:
    MainFrame();
};
