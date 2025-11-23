#include "ProcessModule.h"
#include <windows.h>
#include <tlhelp32.h> 
#include <psapi.h>    // hàm GetProcessMemoryInfo
#include <iostream>
#include <algorithm>
#include <iomanip>    
#include <sstream>    

#pragma comment(lib, "psapi.lib")


// Hàm kiểm tra xem chuỗi nhập vào có phải là số k? -> "1234" -> true, "notepad" -> false 
bool ProcessModule::IsNumeric(const std::string& str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

// Hàm tìm PID dựa trên Tên Process
DWORD ProcessModule::FindProcessId(const std::string& processName) {
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    DWORD pid = 0;

    // Chụp ảnh (Snapshot) toàn bộ danh sách process đang chạy
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return 0;

    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Lấy process ĐẦU TIÊN trong danh sách
    if (Process32First(hProcessSnap, &pe32)) {
        do {
            // Chuyển tên file từ dạng WCHAR (Unicode) sang string thường
            std::wstring wExeName = pe32.szExeFile;
            std::string currentName(wExeName.begin(), wExeName.end());

            if (currentName == processName) {
                pid = pe32.th32ProcessID;
                break;
            }
            // Tiếp tục duyệt sang process TIẾP THEO
        } while (Process32Next(hProcessSnap, &pe32));
    }

    CloseHandle(hProcessSnap);
    return pid;
}

// Hàm lấy dung lượng RAM (MB) mà một PID đang sử dụng
double ProcessModule::GetProcessMemory(DWORD pid) {
    PROCESS_MEMORY_COUNTERS pmc;

    // Mở process với quyền "QUERY_INFORMATION" (để hỏi thông tin) và "VM_READ" (đọc bộ nhớ ảo)
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

    if (NULL == hProcess) return 0.0; // Không mở được (thường do thiếu quyền Admin)

    // Lấy thông tin bộ nhớ
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        CloseHandle(hProcess);
        // WorkingSetSize là byte -> chia cho (1024*1024) để ra Megabyte (MB)
        return (double)pmc.WorkingSetSize / (1024 * 1024);
    }

    CloseHandle(hProcess);
    return 0.0;
}



// LIST PROCESS
std::string ProcessModule::ListProcesses() {
    std::string result = "PID\tRAM(MB)\tThreads\tName\n"; 
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    // Chụp snapshot danh sách process
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return "Error snapshot";

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return "Error process info";
    }

    do {
        std::wstring wExeName = pe32.szExeFile;
        std::string exeName(wExeName.begin(), wExeName.end());

        // lấy RAM
        double ramUsage = GetProcessMemory(pe32.th32ProcessID);

        std::stringstream ss;
        ss << pe32.th32ProcessID << "\t"
            << std::fixed << std::setprecision(1) << ramUsage << "\t"
            << pe32.cntThreads << "\t"
            << exeName << "\n";

        result += ss.str();
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return result;
}

// START PROCESS (Đã bỏ tham số ẩn/hiện)
bool ProcessModule::StartProcess(const std::string& pathOrName) {
    // ShellExecuteA: Tương đương lệnh "Run" của Windows
    // Tham số: "open" (hành động mở), SW_SHOWNORMAL (Hiện cửa sổ bình thường)
    HINSTANCE result = ShellExecuteA(NULL, "open", pathOrName.c_str(), NULL, NULL, SW_SHOWNORMAL);

    // Kết quả > 32 là thành công
    return ((intptr_t)result > 32);
}

// STOP PROCESS
std::string ProcessModule::StopProcess(const std::string& nameOrPid) {
    DWORD pid = 0;

    // Nếu nhập số -> Convert sang PID. Nếu nhập tên -> Tìm PID từ tên.
    if (IsNumeric(nameOrPid)) pid = std::stoul(nameOrPid);
    else pid = FindProcessId(nameOrPid);

    if (pid == 0) return "Error: Process not found.";

    // Xin quyền hệ điều hành để TERMINATE (Kill) process này
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);

    if (hProcess == NULL) {
        // Nếu không mở được, lấy mã lỗi để báo cáo
        DWORD error = GetLastError();
        if (error == 5) return "Error: Access Denied."; // Lỗi Không đủ quyền Admin
        return "Error: Cannot open process (" + std::to_string(error) + ").";
    }

    // Thực hiện lệnh Kill (Exit code 1)
    if (TerminateProcess(hProcess, 1)) {
        CloseHandle(hProcess);
        return "Success: Process killed.";
    }

    CloseHandle(hProcess);
    return "Error: Unknown failure.";
}