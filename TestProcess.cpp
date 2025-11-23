#include <iostream>
#include <string>
#include "ProcessModule.h"

int main() {
    ProcessModule pm;
    int choice = -1;
    std::string input;

    while (true) {
        std::cout << "\n=== PROCESS MODULE (HUY - SIMPLE) ===\n";
        std::cout << "1. List (PID - RAM - Threads - Name)\n";
        std::cout << "2. Start (Nhap ten la chay)\n";
        std::cout << "3. Stop (Bao loi chi tiet)\n";
        std::cout << "0. Exit\n";
        std::cout << "Chon: ";

        if (!(std::cin >> choice)) {
            std::cin.clear(); std::cin.ignore(1000, '\n'); choice = -1;
        }
        std::cin.ignore();

        if (choice == 0) break;
        else if (choice == 1) {
            std::cout << "Dang quet he thong...\n";
            std::cout << pm.ListProcesses() << "\n";
        }
        else if (choice == 2) {
            std::cout << "Nhap ten chuong trinh (vd: notepad.exe): ";
            std::getline(std::cin, input);

            if (pm.StartProcess(input))
                std::cout << "-> Start thanh cong!\n";
            else
                std::cout << "-> Start that bai.\n";
        }
        else if (choice == 3) {
            std::cout << "Nhap Ten hoac PID: ";
            std::getline(std::cin, input);
            std::cout << "-> " << pm.StopProcess(input) << "\n";
        }
    }
    return 0;
}