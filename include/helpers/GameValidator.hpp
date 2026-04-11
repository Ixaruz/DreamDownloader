#pragma once
#include <switch.h>
#include <string>

class GameValidator
{
public:
    GameValidator();
    ~GameValidator();

    bool validateGame();
    bool validateVersion();

private:
    u64 titleId_, processId_;
    NsApplicationControlData controlData_;
    std::string versionString_;
};