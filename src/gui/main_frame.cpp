#include "gui/main_frame.hpp"
#include "common/number_input.hpp"

#include <wx/wx.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/notebook.h>
#include <wx/statline.h>

#include <limits>
#include <charconv>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
wxString ui(const std::string& text) { return wxString::FromUTF8(text.c_str()); }
std::string text(const wxTextCtrl* field) { return field->GetValue().utf8_string(); }

wxString gpaText(double gpa) {
    char buffer[64]{};
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), gpa);
    return wxString::FromUTF8(buffer, static_cast<std::size_t>(converted.ptr - buffer));
}

void errorDialog(wxWindow* parent, const Error& error) {
    wxMessageBox(ui(formatError(error)), "Operation failed", wxOK | wxICON_ERROR, parent);
}

template <typename Sizer>
void attachSizer(wxSizer& parent, std::unique_ptr<Sizer>& child, int proportion, int flags, int border = 0) {
    if (!parent.Add(child.get(), proportion, flags, border)) {
        throw std::runtime_error("Cannot attach a layout");
    }
    child.release(); // NOLINT(bugprone-unused-return-value,cert-err33-c) wxWidgets owns the sizer
}

template <typename Sizer>
void installSizer(wxWindow& window, std::unique_ptr<Sizer>& layout) {
    window.SetSizer(layout.get());
    layout.release(); // NOLINT(bugprone-unused-return-value,cert-err33-c) The window owns the sizer
}

void appendMenu(wxMenuBar& menuBar, std::unique_ptr<wxMenu>& menu, const wxString& title) {
    if (!menuBar.Append(menu.get(), title)) {
        throw std::runtime_error("Cannot attach a menu");
    }
    menu.release(); // NOLINT(bugprone-unused-return-value,cert-err33-c) The menu bar owns the menu
}

wxButton* button(wxWindow* parent, wxSizer* sizer, const wxString& label,
                 const wxString& name, const std::function<void()>& action) {
    auto* result = new wxButton(parent, wxID_ANY, label);
    result->SetName(name);
    result->Bind(wxEVT_BUTTON, [action](wxCommandEvent&) { action(); });
    sizer->Add(result, 0, wxRIGHT, 8);
    return result;
}

wxTextCtrl* input(wxWindow* parent, wxSizer* sizer, const wxString& label,
                  const wxString& value, const wxString& name, wxStaticText** labelControl = nullptr) {
    auto group = std::make_unique<wxBoxSizer>(wxVERTICAL);
    auto* caption = new wxStaticText(parent, wxID_ANY, label);
    if (labelControl) *labelControl = caption;
    group->Add(caption, 0, wxBOTTOM, 4);
    auto* field = new wxTextCtrl(parent, wxID_ANY, value);
    field->SetName(name);
    group->Add(field, 1, wxEXPAND);
    attachSizer(*sizer, group, 1, wxRIGHT, 12);
    return field;
}

using FormFields = DynamicArray<TwoElement<std::string, std::string>>;
using SubmitForm = std::function<std::expected<void, Error>(const DynamicArray<std::string>&)>;

void showForm(wxWindow* parent, const wxString& title, const FormFields& fields, SubmitForm submit) {
    wxDialog dialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                    wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    dialog.SetName("input-dialog");
    auto layout = std::make_unique<wxBoxSizer>(wxVERTICAL);
    auto grid = std::make_unique<wxFlexGridSizer>(2, 10, 12);
    grid->AddGrowableCol(1);
    DynamicArray<wxTextCtrl*> controls;
    for (const auto& field : fields) {
        grid->Add(new wxStaticText(&dialog, wxID_ANY, ui(field.first)), 0, wxALIGN_CENTER_VERTICAL);
        auto* control = new wxTextCtrl(&dialog, wxID_ANY, ui(field.second));
        control->SetName(ui(field.first));
        controls.push_back(control);
        grid->Add(control, 1, wxEXPAND);
    }
    attachSizer(*layout, grid, 1, wxEXPAND | wxALL, 16);
    std::unique_ptr<wxSizer> buttons(dialog.CreateSeparatedButtonSizer(wxOK | wxCANCEL));
    if (buttons) attachSizer(*layout, buttons, 0, wxEXPAND | wxALL, 12);
    installSizer(dialog, layout);
    dialog.GetSizer()->SetSizeHints(&dialog);
    dialog.SetMinSize(dialog.FromDIP(wxSize(420, 180)));
    dialog.Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
        DynamicArray<std::string> values;
        for (const auto* control : controls) values.push_back(text(control));
        const auto result = submit(values);
        if (!result) {
            errorDialog(&dialog, result.error());
            for (auto* control : controls) {
                if (control->GetName() == ui(result.error().context.field)) {
                    control->SetFocus();
                    control->SelectAll();
                    break;
                }
            }
            return;
        }
        dialog.EndModal(wxID_OK);
    }, wxID_OK);
    controls[0]->SetFocus();
    dialog.CentreOnParent();
    dialog.ShowModal();
}

DynamicArray<Student> resolveMatches(const StudentSearchService& service, const DynamicArray<RecordId>& ids) {
    DynamicArray<Student> result(ids.size());
    for (const auto id : ids) {
        const auto student = service.getRepository().findById(id);
        if (!student) throw std::logic_error("Search returned an unknown student ID");
        result.push_back(*student);
    }
    return result;
}
}

StudentTable::StudentTable(wxWindow* parent, const wxString& name)
    : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                 wxLC_REPORT | wxLC_VIRTUAL | wxLC_HRULES) {
    SetName(name);
    InsertColumn(0, "ID", wxLIST_FORMAT_LEFT, FromDIP(120));
    InsertColumn(1, "First name", wxLIST_FORMAT_LEFT, FromDIP(200));
    InsertColumn(2, "Last name", wxLIST_FORMAT_LEFT, FromDIP(200));
    InsertColumn(3, "Birth year", wxLIST_FORMAT_LEFT, FromDIP(110));
    InsertColumn(4, "GPA", wxLIST_FORMAT_LEFT, FromDIP(130));
}

wxString StudentTable::OnGetItemText(long item, long column) const {
    const auto& student = rows_.at(static_cast<std::size_t>(item));
    switch (column) {
        case 0: return ui(std::to_string(student.id));
        case 1: return ui(student.firstName);
        case 2: return ui(student.lastName);
        case 3: return ui(std::to_string(student.birthYear));
        case 4: return gpaText(student.gpa);
        default: return {};
    }
}

void StudentTable::setRows(DynamicArray<Student> rows) {
    if (rows.size() > static_cast<std::size_t>(std::numeric_limits<long>::max())) {
        throw std::length_error("Too many records for the table");
    }
    Freeze();
    SetItemCount(0);
    rows_ = std::move(rows);
    SetItemCount(static_cast<long>(rows_.size()));
    Refresh();
    Thaw();
}

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "DOR2 - Student Indexing", wxDefaultPosition, wxSize(1120, 760)) {
    SetMinSize(FromDIP(wxSize(840, 620)));
    SetSize(FromDIP(wxSize(1120, 760)));
    auto file = std::make_unique<wxMenu>();
    file->Append(wxID_OPEN, "&Open...\tCtrl+O");
    file->Append(wxID_SAVEAS, "&Save as...\tCtrl+Shift+S");
    file->AppendSeparator();
    file->Append(wxID_EXIT, "E&xit\tCtrl+Q");
    auto help = std::make_unique<wxMenu>();
    help->Append(wxID_ABOUT, "&About");
    auto menu = std::make_unique<wxMenuBar>();
    appendMenu(*menu, file, "&File");
    appendMenu(*menu, help, "&Help");
    SetMenuBar(menu.get());
    menu.release(); // NOLINT(bugprone-unused-return-value,cert-err33-c) The frame owns the menu bar
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { loadStudents(); }, wxID_OPEN);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { saveStudents(); }, wxID_SAVEAS);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        wxMessageBox("Student records, indexes and search algorithm comparison.\n"
                     "Exact search: linear, tree and binary.\nRange search: linear and binary.",
                     "About DOR2", wxOK | wxICON_INFORMATION, this);
    }, wxID_ABOUT);

    auto layout = std::make_unique<wxBoxSizer>(wxVERTICAL);
    notebook_ = new wxNotebook(this, wxID_ANY);
    notebook_->AddPage(createStudentsPage(), "Students");
    notebook_->AddPage(createSearchPage(), "Search");
    notebook_->AddPage(createIndexesPage(), "Indexes");
    notebook_->AddPage(createBenchmarkPage(), "Algorithm comparison");
    layout->Add(notebook_, 1, wxEXPAND | wxALL, 12);
    installSizer(*this, layout);
    CreateStatusBar(2);
    const int widths[] = {-1, FromDIP(300)};
    SetStatusWidths(2, widths);
    snapshot_ = takeSnapshot(application_);
    refreshSnapshot();
    SetStatusText("Ready");
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::onClose, this);
    Centre();
}

wxPanel* MainFrame::createStudentsPage() {
    auto* page = new wxPanel(notebook_);
    auto layout = std::make_unique<wxBoxSizer>(wxVERTICAL);
    auto actions = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    button(page, actions.get(), "Add student...", "add-student", [this] { addStudent(); });
    button(page, actions.get(), "Generate data...", "generate-students", [this] { generateStudents(); });
    button(page, actions.get(), "Open...", "load-students", [this] { loadStudents(); });
    button(page, actions.get(), "Save as...", "save-students", [this] { saveStudents(); });
    attachSizer(*layout, actions, 0, wxALL, 12);
    recordCount_ = new wxStaticText(page, wxID_ANY, "No records. Add a student, generate data or open a file.");
    layout->Add(recordCount_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    students_ = new StudentTable(page, "students-table");
    layout->Add(students_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    installSizer(*page, layout);
    return page;
}

wxPanel* MainFrame::createSearchPage() {
    auto* page = new wxPanel(notebook_);
    searchPanel_ = page;
    auto layout = std::make_unique<wxBoxSizer>(wxVERTICAL);
    auto options = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    options->Add(new wxStaticText(page, wxID_ANY, "Mode"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    searchMode_ = new wxChoice(page, wxID_ANY);
    searchMode_->SetName("search-mode");
    searchMode_->Append("Exact");
    searchMode_->Append("Range");
    searchMode_->SetSelection(0);
    options->Add(searchMode_, 1, wxRIGHT, 16);
    options->Add(new wxStaticText(page, wxID_ANY, "Attribute"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    searchAttribute_ = new wxChoice(page, wxID_ANY);
    searchAttribute_->SetName("search-attribute");
    options->Add(searchAttribute_, 1, wxRIGHT, 16);
    options->Add(new wxStaticText(page, wxID_ANY, "Strategy"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    searchStrategy_ = new wxChoice(page, wxID_ANY);
    searchStrategy_->SetName("search-strategy");
    options->Add(searchStrategy_, 1);
    attachSizer(*layout, options, 0, wxEXPAND | wxALL, 12);
    lowerLabel_ = new wxStaticText(page, wxID_ANY, "Value");
    layout->Add(lowerLabel_, 0, wxLEFT | wxRIGHT, 12);
    auto lowerFields = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    lowerFields_ = lowerFields.get();
    lowerValue_ = input(page, lowerFields_, "ID", "", "search-lower", &lowerValueLabel_);
    lowerFirstName_ = input(page, lowerFields_, "First name", "", "search-lower-first-name");
    lowerYear_ = input(page, lowerFields_, "Birth year", "2003", "search-lower-year");
    attachSizer(*layout, lowerFields, 0, wxEXPAND | wxALL, 12);
    auto upperFields = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    upperFields_ = upperFields.get();
    upperValue_ = input(page, upperFields_, "ID (upper bound)", "", "search-upper", &upperValueLabel_);
    upperFirstName_ = input(page, upperFields_, "First name", "", "search-upper-first-name");
    upperYear_ = input(page, upperFields_, "Birth year", "2004", "search-upper-year");
    attachSizer(*layout, upperFields, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    searchHint_ = new wxStaticText(page, wxID_ANY, "");
    layout->Add(searchHint_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    auto actions = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    button(page, actions.get(), "Search", "search", [this] { search(); });
    button(page, actions.get(), "Rebuild indexes", "search-build-indexes", [this] { buildIndexes(); });
    matchCount_ = new wxStaticText(page, wxID_ANY, "Run a search to view matching students.");
    actions->Add(matchCount_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 12);
    attachSizer(*layout, actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    matches_ = new StudentTable(page, "matches-table");
    layout->Add(matches_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    installSizer(*page, layout);
    searchMode_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { configureSearch(true); });
    searchAttribute_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { configureSearch(false); });
    configureSearch(true);
    return page;
}

void MainFrame::configureSearch(bool resetChoices) {
    const bool range = searchMode_->GetSelection() == 1;
    if (resetChoices) {
        searchAttribute_->Clear();
        if (!range) {
            searchAttribute_->Append("ID");
            searchAttribute_->Append("Last name");
        }
        searchAttribute_->Append("Birth year");
        searchAttribute_->Append("GPA");
        searchAttribute_->Append("Composite key");
        searchAttribute_->SetSelection(0);
        searchStrategy_->Clear();
        searchStrategy_->Append("Linear");
        if (!range) searchStrategy_->Append("Tree");
        searchStrategy_->Append("Binary");
        searchStrategy_->SetSelection(0);
    }
    const wxString attribute = searchAttribute_->GetStringSelection();
    const bool composite = attribute == "Composite key";
    const wxString valueLabel = composite ? wxString("Last name") : attribute;
    lowerValueLabel_->SetLabel(valueLabel);
    upperValueLabel_->SetLabel(valueLabel + " (upper bound)");
    lowerLabel_->SetLabel(range ? "Lower bound (inclusive)" : "Exact value");
    searchPanel_->GetSizer()->Show(upperFields_, range);
    lowerFields_->Show(std::size_t{1}, composite);
    lowerFields_->Show(std::size_t{2}, composite);
    upperFields_->Show(std::size_t{1}, range && composite);
    upperFields_->Show(std::size_t{2}, range && composite);
    searchHint_->SetLabel(composite ?
        "Composite keys are compared by last name, then first name, then birth year. Both bounds are inclusive." :
        "Use a dot for decimals. Indexed strategies require current indexes. Range bounds are inclusive.");
    searchPanel_->Layout();
}

wxPanel* MainFrame::createIndexesPage() {
    auto* page = new wxPanel(notebook_);
    auto layout = std::make_unique<wxBoxSizer>(wxVERTICAL);
    indexState_ = new wxStaticText(page, wxID_ANY, "Indexes: not built");
    indexState_->SetName("index-state");
    layout->Add(indexState_, 0, wxALL, 12);
    auto actions = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    button(page, actions.get(), "Rebuild indexes", "build-indexes", [this] { buildIndexes(); });
    attachSizer(*layout, actions, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    statistics_ = new wxListCtrl(page, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
    statistics_->SetName("index-statistics");
    statistics_->InsertColumn(0, "Statistic", wxLIST_FORMAT_LEFT, FromDIP(280));
    statistics_->InsertColumn(1, "Count", wxLIST_FORMAT_LEFT, FromDIP(150));
    layout->Add(statistics_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    installSizer(*page, layout);
    return page;
}

wxPanel* MainFrame::createBenchmarkPage() {
    auto* page = new wxPanel(notebook_);
    auto layout = std::make_unique<wxBoxSizer>(wxVERTICAL);
    auto fields = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    datasetSize_ = input(page, fields.get(), "Dataset size (0-10000)", "1000", "benchmark-size");
    queryCount_ = input(page, fields.get(), "Query count (0-10000)", "200", "benchmark-queries");
    benchmarkSeed_ = input(page, fields.get(), "Seed", "100", "benchmark-seed");
    attachSizer(*layout, fields, 0, wxEXPAND | wxALL, 12);
    layout->Add(new wxStaticText(page, wxID_ANY,
        "Dataset size x query count must not exceed 10000000. Comparison uses a separate dataset."),
        0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    auto actions = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    button(page, actions.get(), "Run comparison", "run-benchmark", [this] { runBenchmark(); });
    button(page, actions.get(), "Export CSV...", "export-benchmark", [this] { exportBenchmark(); });
    attachSizer(*layout, actions, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    benchmark_ = new wxListCtrl(page, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
    benchmark_->SetName("benchmark-table");
    const wxString columns[] = {"Dataset size", "Input order", "Algorithm", "Operation", "Queries",
                                "Time (us)", "Matches", "Seed", "Verified"};
    for (int i = 0; i < 9; ++i) benchmark_->InsertColumn(i, columns[i], wxLIST_FORMAT_LEFT, FromDIP(i == 2 ? 180 : 115));
    layout->Add(benchmark_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    installSizer(*page, layout);
    return page;
}

void MainFrame::showError(const Error& error) {
    lastError_ = error;
    if (!suppressErrorDialogs_) errorDialog(this, error);
}

void MainFrame::addStudent() {
    if (busy_) return;
    showForm(this, "Add student", {{"ID", ""}, {"First name", ""}, {"Last name", ""},
                                  {"Birth year", "2003"}, {"GPA", "4.0"}},
        [this](const DynamicArray<std::string>& values) -> std::expected<void, Error> {
            const auto id = parseNumber<RecordId>(values[0], "ID");
            const auto year = parseNumber<int>(values[3], "Birth year");
            const auto gpa = parseNumber<double>(values[4], "GPA");
            if (!id) return std::unexpected(id.error());
            if (!year) return std::unexpected(year.error());
            if (!gpa) return std::unexpected(gpa.error());
            Student student{*id, values[1], values[2], *year, *gpa};
            runTask("Student added", [student](StudentApplication& app, TaskOutcome&) {
                return app.addStudent(student);
            }, DataEffect::Changed);
            return {};
        });
}

bool MainFrame::confirmReplace() {
    return snapshot_.students.empty() || wxMessageBox(
        "Replace the current student records? Unsaved changes will be lost.",
        "Replace records", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) == wxYES;
}

void MainFrame::generateStudents() {
    if (busy_) return;
    showForm(this, "Generate student data", {{"Record count", "1000"}, {"Seed", "100"}},
        [this](const DynamicArray<std::string>& values) -> std::expected<void, Error> {
            const auto count = parseBoundedSize(values[0], "Record count", StudentApplication::maxGeneratedRecords);
            const auto seed = parseNumber<std::uint64_t>(values[1], "Seed");
            if (!count) return std::unexpected(count.error());
            if (!seed) return std::unexpected(seed.error());
            if (!confirmReplace()) return {};
            runTask("Data generated", [count, seed](StudentApplication& app, TaskOutcome&) {
                return app.generateStudents(*count, *seed);
            }, DataEffect::Changed);
            return {};
        });
}

void MainFrame::loadStudents() {
    if (busy_) return;
    wxFileDialog dialog(this, "Open student data", "", "", "Student data (*.txt)|*.txt|All files|*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK || !confirmReplace()) return;
    const std::filesystem::path path(dialog.GetPath().utf8_string());
    runTask("Data loaded and indexes rebuilt", [path](StudentApplication& app, TaskOutcome&) {
        return app.loadStudents(path);
    }, DataEffect::Loaded);
}

void MainFrame::saveStudents() {
    if (busy_) return;
    wxFileDialog dialog(this, "Save student data", "", "students.txt", "Student data (*.txt)|*.txt|All files|*",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const std::filesystem::path path(dialog.GetPath().utf8_string());
    runTask("Data saved", [path](StudentApplication& app, TaskOutcome&) {
        return app.saveStudents(path);
    }, DataEffect::Saved);
}

void MainFrame::buildIndexes() {
    runTask("Indexes rebuilt", [](StudentApplication& app, TaskOutcome&) { return app.buildIndexes(); });
}

void MainFrame::search() {
    if (busy_) return;
    SearchRequest request;
    request.range = searchMode_->GetSelection() == 1;
    const SearchAttribute exactAttributes[] = {SearchAttribute::Id, SearchAttribute::LastName,
        SearchAttribute::BirthYear, SearchAttribute::Gpa, SearchAttribute::CompositeKey};
    const SearchAttribute rangeAttributes[] = {SearchAttribute::BirthYear, SearchAttribute::Gpa, SearchAttribute::CompositeKey};
    request.attribute = request.range ? rangeAttributes[searchAttribute_->GetSelection()] :
                                        exactAttributes[searchAttribute_->GetSelection()];
    request.exactStrategy = static_cast<ExactSearchStrategy>(searchStrategy_->GetSelection());
    request.rangeStrategy = searchStrategy_->GetSelection() == 0 ? RangeSearchStrategy::Linear : RangeSearchStrategy::Binary;
    request.lower = {text(lowerValue_), text(lowerFirstName_), text(lowerYear_)};
    request.upper = {text(upperValue_), text(upperFirstName_), text(upperYear_)};
    matches_->setRows({});
    matchCount_->SetLabel("Searching...");
    runTask("Search complete", [request](StudentApplication& app, TaskOutcome& outcome) -> std::expected<void, Error> {
        const auto result = app.search(request);
        if (!result) return std::unexpected(result.error());
        outcome.matches = resolveMatches(app.getSearchService(), *result);
        return {};
    }, DataEffect::Search);
}

void MainFrame::runBenchmark() {
    if (busy_) return;
    const auto size = parseBoundedSize(text(datasetSize_), "Dataset size", BenchmarkRunner::maxDatasetSize);
    const auto queries = parseBoundedSize(text(queryCount_), "Query count", BenchmarkRunner::maxQueryCount);
    const auto seed = parseNumber<std::uint64_t>(text(benchmarkSeed_), "Seed");
    if (!size) { showError(size.error()); return; }
    if (!queries) { showError(queries.error()); return; }
    if (!seed) { showError(seed.error()); return; }
    if (*size != 0 && *queries > BenchmarkRunner::maxWorkload / *size) {
        showError(makeError(ErrorCode::InvalidWorkload));
        return;
    }
    runTask("Comparison complete", [size, queries, seed](StudentApplication& app, TaskOutcome&) {
        return app.runBenchmark(*size, *queries, *seed);
    });
}

void MainFrame::exportBenchmark() {
    if (busy_) return;
    if (snapshot_.benchmark.empty()) { showError(makeError(ErrorCode::NoBenchmarkResults)); return; }
    wxFileDialog dialog(this, "Export comparison", "", "benchmark_results.csv", "CSV files (*.csv)|*.csv|All files|*",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const std::filesystem::path path(dialog.GetPath().utf8_string());
    runTask("Comparison exported", [path](StudentApplication& app, TaskOutcome&) {
        return app.exportBenchmark(path);
    });
}

MainFrame::Snapshot MainFrame::takeSnapshot(const StudentApplication& app) {
    Snapshot result;
    const auto& service = app.getSearchService();
    const auto& repository = service.getRepository();
    result.students = DynamicArray<Student>(repository.getCount());
    for (std::size_t i = 0; i < repository.getCount(); ++i) result.students.push_back(repository.at(i));
    for (const auto& row : app.getLatestResults()) result.benchmark.push_back(row);
    const auto statistics = service.getIndexStatistics();
    if (statistics) result.statistics = *statistics;
    result.indexState = !service.getIndexes().isBuilt() ? "not built" :
                       service.getIndexes().isStale(repository) ? "out of date" : "ready";
    return result;
}

void MainFrame::refreshSnapshot() {
    students_->setRows(snapshot_.students);
    recordCount_->SetLabel(snapshot_.students.empty() ?
        "No records. Add a student, generate data or open a file." :
        ui("Records: " + std::to_string(snapshot_.students.size())));
    indexState_->SetLabel(ui("Indexes: " + snapshot_.indexState));
    SetStatusText(ui("Records: " + std::to_string(snapshot_.students.size()) + " | Indexes: " + snapshot_.indexState), 1);
    statistics_->DeleteAllItems();
    if (snapshot_.statistics) {
        const auto& stats = *snapshot_.statistics;
        const TwoElement<std::string, std::size_t> rows[] = {
            {"Records / ID keys", stats.recordCount}, {"Last-name keys", stats.lastNameKeyCount},
            {"Birth-year keys", stats.birthYearKeyCount}, {"GPA keys", stats.gpaKeyCount},
            {"Composite keys", stats.compositeKeyCount}};
        for (const auto& row : rows) {
            const auto index = statistics_->InsertItem(statistics_->GetItemCount(), ui(row.first));
            statistics_->SetItem(index, 1, ui(std::to_string(row.second)));
        }
    }
    benchmark_->Freeze();
    benchmark_->DeleteAllItems();
    for (const auto& row : snapshot_.benchmark) {
        const auto index = benchmark_->InsertItem(benchmark_->GetItemCount(), ui(std::to_string(row.datasetSize)));
        benchmark_->SetItem(index, 1, ui(row.inputOrder));
        benchmark_->SetItem(index, 2, ui(row.algorithm));
        benchmark_->SetItem(index, 3, ui(row.operation));
        benchmark_->SetItem(index, 4, ui(std::to_string(row.queryCount)));
        benchmark_->SetItem(index, 5, wxString::Format("%.3f", row.elapsedUs));
        benchmark_->SetItem(index, 6, ui(std::to_string(row.matchCount)));
        benchmark_->SetItem(index, 7, ui(std::to_string(row.seed)));
        benchmark_->SetItem(index, 8, row.verified ? "Yes" : "FAILED");
    }
    benchmark_->Thaw();
    searchPanel_->Layout();
}

void MainFrame::setBusy(bool busy, const std::string& message) {
    busy_ = busy;
    notebook_->Enable(!busy);
    GetMenuBar()->Enable(wxID_OPEN, !busy);
    GetMenuBar()->Enable(wxID_SAVEAS, !busy);
    SetStatusText(ui(message));
}

void MainFrame::runTask(const std::string& label, Action action, DataEffect effect) {
    if (busy_ || closeAfterTask_) return;
    if (worker_.joinable()) worker_.join();
    lastError_.reset();
    setBusy(true, "Working...");
    try {
        auto outcome = std::make_shared<TaskOutcome>();
        worker_ = std::jthread([this, label, action = std::move(action), effect, outcome] {
            try {
                const auto result = action(application_, *outcome);
                if (!result) outcome->error = result.error();
            } catch (const std::exception& error) {
                outcome->error = makeError(ErrorCode::Unexpected, {.detail = error.what()});
            } catch (...) { outcome->error = makeError(ErrorCode::Unexpected); }
            try { outcome->snapshot = takeSnapshot(application_); }
            catch (const std::exception& error) {
                outcome->error = makeError(ErrorCode::Unexpected, {.detail = error.what()});
            } catch (...) { outcome->error = makeError(ErrorCode::Unexpected); }
            CallAfter([this, outcome, label, effect] {
                if (worker_.joinable()) worker_.join();
                setBusy(false, outcome->error ? "Operation failed" : label);
                if (outcome->snapshot) {
                    snapshot_ = std::move(*outcome->snapshot);
                    refreshSnapshot();
                }
                if (!outcome->error) {
                    if (effect == DataEffect::Changed || effect == DataEffect::Loaded) {
                        dirty_ = effect == DataEffect::Changed;
                        matches_->setRows({});
                        matchCount_->SetLabel("Data changed. Run a search to view current results.");
                    } else if (effect == DataEffect::Saved) dirty_ = false;
                    if (outcome->matches) {
                        matchCount_->SetLabel(ui("Matches: " + std::to_string(outcome->matches->size())));
                        matches_->setRows(std::move(*outcome->matches));
                    }
                }
                if (outcome->error) {
                    if (effect == DataEffect::Search) matchCount_->SetLabel("Search failed. Check the error details.");
                    showError(*outcome->error);
                }
                if (closeAfterTask_) { closeAfterTask_ = false; Close(); }
            });
        });
    } catch (const std::exception& error) {
        setBusy(false, "Operation failed");
        showError(makeError(ErrorCode::Unexpected, {.detail = error.what()}));
    }
}

void MainFrame::onClose(wxCloseEvent& event) {
    if (busy_ && event.CanVeto()) {
        closeAfterTask_ = true;
        SetStatusText("Finishing the current operation before closing...");
        event.Veto();
        return;
    }
    if (worker_.joinable()) worker_.join();
    if (dirty_ && event.CanVeto() && wxMessageBox("Close without saving student records?", "Unsaved records",
        wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES) {
        event.Veto();
        return;
    }
    event.Skip();
}
