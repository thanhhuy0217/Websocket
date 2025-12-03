#include <iostream>
#include <cstdlib>

void DoRestart()
{
    std::cout << "[SYSTEM] Executing Restart command...\n";
    // Lenh CMD: khoi dong lai (/r) sau 15 giay (/t 15)
    system("shutdown /r /t 15");
}