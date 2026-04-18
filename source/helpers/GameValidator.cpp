#include <helpers/GameValidator.hpp>

#include <map>
#include <string.h>
#include <stdio.h>

namespace // anonymous
{
    u64 const supportedTitleId = 0x01006F8002326000; // ACNH...


    std::map<std::string, u64> const knownSupportedVersionsAndTokenOffset = {
        {"3.0.0", 0x4B80678},
        {"3.0.1", 0x4B80678},
        {"3.0.2", 0x4B81678},
    };

}

GameValidator::GameValidator() :
    titleId_(0),
    processId_(0),
    versionString_("")
{
    pminfoInitialize();
    pmdmntInitialize();
    nsInitialize();
    memset(&controlData_, 0x00, sizeof(controlData_));
};

bool GameValidator::validateGame()
{
    pmdmntGetApplicationProcessId(&processId_);
    pminfoGetProgramId(&titleId_, processId_);

    if (titleId_ != supportedTitleId) {
        return false;
    }

    return true;
}

u64 GameValidator::getTokenOffset()
{
    size_t actualSize = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_StorageOnly, titleId_, &controlData_, sizeof(controlData_), &actualSize);
    if(R_SUCCEEDED(rc)) {
        versionString_ = std::string(controlData_.nacp.display_version);
        for (const auto& supportedVersion : knownSupportedVersionsAndTokenOffset) {
            if (versionString_ == supportedVersion.first) {
                return supportedVersion.second;
            }
        }
    }
    else {
        printf("Failed to get application control data: %d\n", R_DESCRIPTION(rc));
    }
    return 0;
}

GameValidator::~GameValidator()
{
    nsExit();
    pminfoExit();
    pmdmntExit();
}