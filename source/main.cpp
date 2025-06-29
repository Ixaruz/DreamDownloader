#include <version.h>
#include <meta.h>
#include <net/AcbaaWebServer.hpp>
#include <helpers/BuildIdHelper.hpp>

// Include the main libnx system header, for Switch development
#include <switch.h>

// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

void initSwitchModules()
{
    // Initialize the sockets service (needed for networking)
    Result r = socketInitializeDefault();
    if (R_FAILED(r))
        printf("ERROR initializing socket: %d\n", R_DESCRIPTION(r));

    signed int nxlinkStdioR = nxlinkStdio();
    if (nxlinkStdioR < 0)
        printf("ERROR initializing nxlinkStdio: %d\n", nxlinkStdioR);
        
    r = setsysInitialize();
    if (R_FAILED(r))
        printf("ERROR initializing setsys: %d\n", R_DESCRIPTION(r));

    r = dmntchtInitialize();
    if (R_FAILED(r))
        printf("ERROR initializing dmnt:cht: %d\n", R_DESCRIPTION(r));
}


// Closes and unloads all libnx modules we need
void exitSwitchModules()
{
    // Exit the loaded modules in reversed order we loaded them
    dmntchtExit();
    setsysExit();
    socketExit();
}

// Main program entrypoint
int main(int argc, char* argv[])
{
    consoleInit(NULL);

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    initSwitchModules();
    
    printf("%s %s\n\n", PROJECT_NAME, PROJECT_VERSION);
    
    dmntchtForceOpenCheatProcess();

    BuildIdHelper::verifyBid();
    
    u64 tokenOffset = 0x5128FE0;
    DmntCheatProcessMetadata metadata;
    dmntchtGetCheatProcessMetadata(&metadata);
    u8 token[0x5c];
    printf("Process ID: %016llx\n", metadata.process_id);
    printf("Title ID: %016llx\n", metadata.title_id);
    printf("Main NSO Base: %016llx\n", metadata.main_nso_extents.base);
    dmntchtReadCheatProcessMemory(metadata.main_nso_extents.base + tokenOffset, &token, sizeof(token));
    
    std::string tokenStr(reinterpret_cast<const char*>(token), sizeof(token) / sizeof(token[0]));

    printf("Token: %s\n", tokenStr.c_str());
    static AcbaaWebServer server(tokenStr);
    if (!tokenStr.empty() && 0 != tokenStr[0])
    {
        
        if (!server.start(SERVER_PORT)) {
            printf("Failed to start server\n");
        }
        else {
            printf("Server running on port %ld...\n", SERVER_PORT);
            
        }
    }
    else {
        printf("Token was empty.");
    }
    // Main loop
    while(appletMainLoop())
    {
        // Scan the gamepad. This should be done once for each frame
        padUpdate(&pad);

        // padGetButtonsDown returns the set of buttons that have been
        // newly pressed in this frame compared to the previous one
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus)
            break; // break in order to return to hbmenu

        // Handle one step of the server (non-blocking)
        server.serverLoop();

        // Update the console, sending a new frame to the display
        consoleUpdate(NULL);
    }

    exitSwitchModules();
    // Deinitialize and clean up resources used by the console (important!)
    consoleExit(NULL);
    return 0;
}
