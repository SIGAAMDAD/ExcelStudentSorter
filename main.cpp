#include "gln.h"
#include "application.h"
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include <memory>
#include "ExcelSorter.h"
//#include "imstb_textedit.h"
//#include "imstb_truetype.h"

static constexpr ImVec2 FileDlgWindowSize = ImVec2( 1012, 641 );

class ExcelSheet
{};


std::unique_ptr<ExcelSorter> sorter;

ExcelSorter::ExcelSorter(void)
{
    mFilePath = "";
    mCurrentPath = std::filesystem::current_path();
}

const char* __attribute__((format(printf, 3, 4))) ExcelSorter::ThrowError(const char *func, const char *fmt, ...)
{
    va_list argptr;
    static char msg[8192];

    memset(msg, 0, sizeof(msg));
    va_start(argptr, fmt);
    vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    mErrorStack.emplace_back(va("%s: %s\n", func, msg));

    return msg;
}

#define Log_Error(...) ThrowError(__PRETTY_FUNCTION__, __VA_ARGS__)

void ExcelSorter::SortFile(xlsxioreader handle)
{
    xlsxioreadersheet sheetPtr;
    xlsxioreadersheetlist sheetList;
    const char *sheetName;

    if ((sheetList = xlsxioread_sheetlist_open(handle)) == NULL) {
        xlsxioread_close(handle);
        throw std::runtime_error(Log_Error("Failed to open a sheetlist for '%s'", mFilePath.c_str()));
    }

    while ((sheetName = xlsxioread_sheetlist_next(sheetList))) {
        mSheetList.try_emplace(sheetName);
    }
    xlsxioread_sheetlist_close(sheetList);

    for (auto it : mSheetList) {
        if ((sheetPtr = xlsxioread_sheet_open(handle, it.first.c_str(), XLSXIOREAD_SKIP_ALL_EMPTY)) == NULL) {
            throw std::runtime_error(Log_Error("Failed to load sheet '%s'", it.first.c_str()));
        }

        while (xlsxioread_sheet_next_row(sheetPtr)) {
            
        }

        xlsxioread_sheet_close(sheetPtr);
    }
}

void ExcelSorter::OpenFileRead(const std::string& filepath)
{
    xlsxioreader handle;

    if ((handle = xlsxioread_open(filepath.c_str())) == NULL) {
        Log_Printf(Log_Error("Failed to load excel file '%s', xlsxioread_open returned NULL", filepath.c_str()));
        return;
    }
    mFilePath = filepath;

    try {
        SortFile(handle);
    } catch (const std::exception& e) {
        Log_Printf("std::exception caught: %s", e.what());
        xlsxioread_close(handle);
        return;
    }

    xlsxioread_close(handle);
}

static void RunWidgets(void)
{
    ImGui::Begin("ExcelStudentSorter##SorterThingyMajig", NULL, ImGuiWindowFlags_NoMove);

    if (ImGui::Button("Select File")) {
        ImGuiFileDialog::Instance()->OpenDialog("SelectExcelFileDlg", "Select File", ".xlsx", sorter->mCurrentPath.string());
    }

    if (sorter->mFilePath.size()) {
        ImGui::SeparatorText(va("Current File: %s", sorter->mFilePath.c_str()));
        if (ImGui::BeginMenu("Select Sorting Variables")) {
            ImGui::EndMenu();
        }
    }

    ImGui::SeparatorText(va("Error Stack (%lu errors)", sorter->mErrorStack.size()));
    if (!sorter->mErrorStack.size()) {
        ImGui::Text("None (Empty)");
    }
    else {
        for (const auto& it : sorter->mErrorStack) {
            ImGui::Text("%s", it.c_str());
        }
    }

    if (ImGuiFileDialog::Instance()->IsOpened("SelectExcelFileDlg")) {
        if (ImGuiFileDialog::Instance()->Display("SelectExcelFileDlg", ImGuiWindowFlags_NoResize, FileDlgWindowSize, FileDlgWindowSize)) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                sorter->OpenFileRead(ImGuiFileDialog::Instance()->GetFilePathName());
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }

    ImGui::End();
}

int main(int argc, char **argv) // unused parms
{
    app = std::make_unique<Application>("Excel Student Sorter", 1980, 1080);
    sorter = std::make_unique<ExcelSorter>();

    app->Run(RunWidgets);

    return 0;
}

int WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR     lpCmdLine,
  int       nShowCmd
)
{
    return main(0, NULL);
}
