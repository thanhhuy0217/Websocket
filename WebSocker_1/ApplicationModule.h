#pragma once
#include<string>
#include<vector>
#include<algorithm>

struct ApplicationEntry {
	std::string id; //id nội bộ
	std::string displayName; //tên hiện thị lên UI
	std::string exeName; //Tên file exe
	std::string startCommand; //Lệnh để start
};

class ApplicationModule {
private:
	std::vector<ApplicationEntry> _apps;
	const std::vector<ApplicationEntry>* findApp(const std::string& appIdorName) const;
public:
	std::string ListApplication();
	bool StartApplication(const std::string& appName);
	bool EndApplication(const std::string& appName);
public:
	static std::string toLower(std::string s) {
		std::transform(s.begin(), s.end(),s.begin(), [](unsigned char c) { return std::tolower(c); });
		return s;
	}
};