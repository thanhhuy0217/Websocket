#pragma once
#include <string>
#include <vector>
#include <windows.h>

class ProcessModule {
private:
    DWORD FindProcessId(const std::string& processName);
    bool IsNumeric(const std::string& str);
    double GetProcessMemory(DWORD pid);

public:
    std::string ListProcesses();
    bool StartProcess(const std::string& pathOrName);
    std::string StopProcess(const std::string& nameOrPid);
};
