#include "ListApplication.h"
#include <limits> // std::numeric_limits
#define NOMINMAX 

int main() {
    // Cho console dùng UTF-8 để in tiếng Việt cho đỡ lỗi font (optional)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    Application app;

    // Lần đầu load danh sách application đã cài
    std::cout << "Dang quet danh sach ung dung da cai (Application = co exe chay duoc)...\n";
    app.LoadInstalledApplications();
    std::cout << "Quet xong.\n\n";

    int choice = -1;

    while (true) {
        std::cout << "=========================\n";
        std::cout << "    APPLICATION MODULE   \n";
        std::cout << "=========================\n";
        std::cout << "1. List Application (liet ke cac ung dung da cai)\n";
        std::cout << "2. Start Application (nhap ten hoac exePath)\n";
        std::cout << "3. Stop Application (nhap DisplayName cua app)\n";
        std::cout << "0. Thoat\n";
        std::cout << "Chon: ";

        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(1000, '\n');
            std::cout << "Nhap khong hop le, vui long thu lai.\n\n";
            continue;
        }
        // clear newline sau khi nhập số
        std::cin.ignore(1000, '\n');

        if (choice == 0) {
            break;
        }

        switch (choice) {
        case 1: {
            // 1) LIST APPLICATION
            app.LoadInstalledApplications();  
            std::cout << "\n=== DANH SACH APPLICATION ===\n";
            app.printListApplication();
            break;
        }
        case 2: {
            // 2) START APPLICATION: nhập tên hoặc exePath
            std::string input;
            std::cout << "Nhap ten app hoac exePath de START\n";
            std::cout << "  - Vi du ten (DisplayName): Google Chrome\n";
            std::cout << "  - Hoac exePath: C\\\\Windows\\\\System32\\\\notepad.exe\n";
            std::cout << "  - Hoac lenh don gian: notepad, calc\n";
            std::cout << "Nhap: ";

            std::getline(std::cin, input);

            bool ok = app.StartApplicationFromInput(input);
            if (ok) {
                std::cout << "-> StartApplication: THANH CONG.\n\n";
            }
            else {
                std::cout << "-> StartApplication: THAT BAI (khong tim thay hoac ShellExecute loi).\n\n";
            }
            break;
        }
        case 3: {
            // 3) STOP APPLICATION: nhập DisplayName
            std::string displayName;
            std::cout << "Nhap DisplayName cua app de STOP (giong nhu ten hien trong List):\n";
            std::cout << "Vi du: Google Chrome\n";
            std::cout << "Nhap: ";

            std::getline(std::cin, displayName);

            bool ok = app.StopApplicationByName(displayName);
            if (ok) {
                std::cout << "-> StopApplication: THANH CONG (khong con process nao chay exe nay hoac app da khong chay).\n\n";
            }
            else {
                std::cout << "-> StopApplication: THAT BAI (khong tim thay app, hoac khong du quyen terminate).\n\n";
            }
            break;
        }
        default:
            std::cout << "Lua chon khong hop le, vui long chon 0-3.\n\n";
            break;
        }
    }

    std::cout << "Thoat chuong trinh test Application.\n";
    return 0;
}
