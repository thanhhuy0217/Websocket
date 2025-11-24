#include"ListApplication.h"
bool Application::StartProcessShell(const std::string& pathOrCommand) {
    if (pathOrCommand.empty()) return false;

    HINSTANCE hInst = ShellExecuteA(
        nullptr,
        "open",
        pathOrCommand.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );

    INT_PTR code = reinterpret_cast<INT_PTR>(hInst);
    return code > 32;
}

bool Application::StartApplicationByName(const std::string& displayName) {
    std::string target = ToLower(displayName);

    for (const auto& app : g_applications) {
        if (ToLower(app.name) == target) {
            if (!app.command.empty()) {
                return StartProcessShell(app.command);
            }
            else {
                return StartProcessShell(app.name);
            }
        }
    }

    return false;
}

bool Application::StartApplicationFromInput(const std::string& inputRaw) {
    std::string input = Trim(inputRaw);
    if (input.empty()) return false;

    // 1) Thử coi input là DisplayName của app đã cài (ListApplication)
    std::string target = ToLowerStr(input);

    for (const auto& app : g_applications) {
        if (ToLowerStr(Trim(app.name)) == target) {
            // tìm thấy app theo DisplayName -> chạy theo exePath (command)
            if (!app.command.empty()) {
                return StartProcessShell(app.command);
            }
        }
    }

    // 2) Nếu không phải DisplayName, coi như user nhập exePath / tên lệnh
    //    giao trực tiếp cho ShellExecute (notepad, notepad.exe, C:\...\zalo.exe, ...).
    return StartProcessShell(input);
}
