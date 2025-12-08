#include "Process.h"
#include <windows.h>
#include <tlhelp32.h> 
#include <psapi.h>    
#include <iostream>
#include <algorithm>
#include <iomanip>    
#include <sstream>    
#include <string>
#include <shlobj.h>
#include <shlwapi.h> 

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")
using namespace std;

string toLowerCase(string str) {
    string res = str;
    for (int i = 0; i < res.length(); i++) {
        res[i] = tolower(res[i]);
    }
    return res;
}

// Kiểm tra chuỗi có phải là số (dùng để phân biệt PID và Tên)
bool Process::IsNumeric(const string& str) {
    if (str.empty()) return false;
    for (char c : str) {
        if (!isdigit(c)) return false;
    }
    return true;
}

// Tìm PID dựa trên Tên Process
DWORD Process::FindProcessId(const string& processName) {
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    DWORD pid = 0;

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return 0;

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hProcessSnap, &pe32)) {
        string nameToFind = toLowerCase(processName);

        do {
            wstring wName = pe32.szExeFile;
            string currentName(wName.begin(), wName.end());

            if (toLowerCase(currentName) == nameToFind) {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hProcessSnap, &pe32));
    }

    CloseHandle(hProcessSnap);
    return pid;
}

// Lấy dung lượng RAM (MB) của process
double Process::GetProcessMemory(DWORD pid) {
    PROCESS_MEMORY_COUNTERS pmc;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

    if (hProcess == NULL) return 0.0;

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        CloseHandle(hProcess);
        return (double)pmc.WorkingSetSize / (1024 * 1024);
    }

    CloseHandle(hProcess);
    return 0.0;
}

// Liệt kê danh sách process đang chạy
string Process::ListProcesses() {
    stringstream ss;
    ss << "PID\tRAM(MB)\tThreads\tName\n";

    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return "Loi snapshot";

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return "Loi doc process";
    }

    do {
        wstring wName = pe32.szExeFile;
        string exeName(wName.begin(), wName.end());

        double ramUsage = GetProcessMemory(pe32.th32ProcessID);

        ss << pe32.th32ProcessID << "\t"
            << fixed << setprecision(1) << ramUsage << "\t"
            << pe32.cntThreads << "\t"
            << exeName << "\n";

    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return ss.str();
}

// Hàm đệ quy tìm file trong thư mục
string FindFileRecursive(string directory, string fileToFind) {
    string searchPath = directory + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) return "";

    string resultPath = "";

    do {
        string currentName = findData.cFileName;
        if (currentName == "." || currentName == "..") continue;

        string fullPath = directory + "\\" + currentName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            string found = FindFileRecursive(fullPath, fileToFind);
            if (!found.empty()) {
                resultPath = found;
                break;
            }
        }
        else {
            string lowerName = toLowerCase(currentName);
            string lowerFind = toLowerCase(fileToFind);

            // Logic tìm kiếm: Chứa từ khóa VÀ phải là file .exe hoặc .lnk
            if (lowerName.find(lowerFind) != string::npos) {
                if (lowerName.find(".lnk") != string::npos || lowerName.find(".exe") != string::npos) {
                    resultPath = fullPath;
                    break;
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return resultPath;
}

// Tìm App trong Start Menu (Hỗ trợ tìm app không nằm trong PATH)
string FindAppInStartMenu(string appName) {
    char path[MAX_PATH];
    string foundPath = "";

    vector<int> foldersToCheck = {
        CSIDL_COMMON_PROGRAMS, // Start Menu (All Users)
        CSIDL_PROGRAMS,        // Start Menu (Current User)
        CSIDL_DESKTOPDIRECTORY // Desktop
    };

    for (int folderId : foldersToCheck) {
        if (SHGetFolderPathA(NULL, folderId, NULL, 0, path) == S_OK) {
            foundPath = FindFileRecursive(string(path), appName);
            if (!foundPath.empty()) return foundPath;
        }
    }

    return "";
}

// --- START PROCESS ---
bool Process::StartProcess(const string& pathOrName) {
    string cmdCommand;
    string input = toLowerCase(pathOrName);

    // Xử lý cắt đuôi .exe (nhập zalo.exe -> tìm zalo)
    string searchKey = input;
    if (searchKey.length() > 4 && searchKey.substr(searchKey.length() - 4) == ".exe") {
        searchKey = searchKey.substr(0, searchKey.length() - 4);
    }

    // CÁCH 1: Thử chạy trực tiếp (App hệ thống hoặc đường dẫn đầy đủ)
    HINSTANCE res = ShellExecuteA(NULL, "open", pathOrName.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)res > 32) return true;

    // CÁCH 2: Quét Start Menu để tìm Shortcut (App bên thứ 3)
    string shortcutPath = FindAppInStartMenu(searchKey);

    if (!shortcutPath.empty()) {
        cmdCommand = "/c start \"\" \"" + shortcutPath + "\"";
        res = ShellExecuteA(NULL, "open", "cmd", cmdCommand.c_str(), NULL, SW_HIDE);
        return ((intptr_t)res > 32);
    }

    return false;
}

// --- STOP PROCESS ---
string Process::StopProcess(const string& nameOrPid) {
    // TH1: Nhập số PID -> Giết đúng PID đó
    if (IsNumeric(nameOrPid)) {
        DWORD pid = stoul(nameOrPid);
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess == NULL) return "Loi: Khong mo duoc PID nay.";

        if (TerminateProcess(hProcess, 1)) {
            CloseHandle(hProcess);
            return "Thanh cong: Da diet PID " + nameOrPid;
        }
        CloseHandle(hProcess);
        return "Loi: Khong diet duoc.";
    }

    // TH2: Nhập Tên Process -> Quét và giết tất cả process trùng tên
    else {
        HANDLE hProcessSnap;
        PROCESSENTRY32 pe32;
        int countKilled = 0;

        hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE) return "Loi snapshot";

        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hProcessSnap, &pe32)) {
            string nameToKill = toLowerCase(nameOrPid);

            do {
                wstring wName = pe32.szExeFile;
                string currentName(wName.begin(), wName.end());

                if (toLowerCase(currentName) == nameToKill) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                    if (hProcess != NULL) {
                        if (TerminateProcess(hProcess, 1)) {
                            countKilled++;
                        }
                        CloseHandle(hProcess);
                    }
                }

            } while (Process32Next(hProcessSnap, &pe32));
        }

        CloseHandle(hProcessSnap);

        if (countKilled > 0) {
            return "Thanh cong: Da diet " + to_string(countKilled) + " process ten " + nameOrPid;
        }
        else {
            return "Loi: Khong tim thay process nao ten " + nameOrPid;
        }
    }
}