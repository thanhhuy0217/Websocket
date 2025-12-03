#include <iostream>
#include <cstdlib>

void DoShutdown()
{
    std::cout << "[SYSTEM] Executing Shutdown command...\n";
    // Lenh CMD: tat may (/s) sau 15 giay (/t 15)
    system("shutdown /s /t 15");
}