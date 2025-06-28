#pragma once

#include <switch/types.h>

#include <string>

// Generic WebServer Interface

class IWebServer {
public:
    virtual ~IWebServer() = default;
    virtual bool start(u16 port) = 0;
    virtual bool serverLoop() = 0;

protected:
    virtual void handleClient(int fd) = 0;
};
