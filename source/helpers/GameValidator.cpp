#include <helpers/GameValidator.hpp>

#include <vector>
#include <string.h>
#include <stdio.h>

namespace // anonymous
{
    u64 const supportedTitleId = 0x01006F8002326000; // ACNH...


    std::vector<std::string> const knownSupportedVersions = {
        "3.0.0",
        "3.0.1",
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

bool GameValidator::validateVersion()
{
    size_t actualSize = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_StorageOnly, titleId_, &controlData_, sizeof(controlData_), &actualSize);
    if(R_SUCCEEDED(rc)) {
        versionString_ = std::string(controlData_.nacp.display_version);
        for (const auto& supportedVersion : knownSupportedVersions) {
            if (versionString_ == supportedVersion) {
                return true;
            }
        }
    }
    else {
        printf("Failed to get application control data: %d\n", R_DESCRIPTION(rc));
    }
    return false;
}

GameValidator::~GameValidator()
{
    nsExit();
    pminfoExit();
    pmdmntExit();
}