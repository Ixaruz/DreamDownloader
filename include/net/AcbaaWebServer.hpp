#pragma once

#include "IWebServer.hpp"
#include "HttpClient.hpp"
#include "HttpRequest.hpp"

#include <unordered_map>
#include <string>
#include <functional>
#include <optional>

class AcbaaWebServer : public HttpClient, public IWebServer {
public:
    AcbaaWebServer(const std::string& bearerToken);
    ~AcbaaWebServer();

    bool start(u16 port) override;
    bool serverLoop() override;

protected:
    void handleClient(int fd) override;
private:
    int m_serverFd;
    bool m_running;

    std::string m_bearerToken;
    std::string m_userAgent;
    std::string m_baseUrl;
    
    // Route -> (ParamKey -> RequestBuilder)
    std::unordered_map<
        std::string,
        std::unordered_map<
            std::string,
            std::function<std::optional<HttpRequest>(const std::string&, const std::unordered_map<std::string, std::string>&)>
        >
    > m_routeRequestBuilders;

    std::unordered_map<std::string, bool> m_routeAuthorizationExemptions;
    
    std::tuple<
    std::string,                                        // method
    std::string,                                        // uri
    std::string,                                        // postBody
    std::unordered_map<std::string, std::string>       // queryParams
    > parseHttpRequest(const std::string& fullReq);
    
    size_t parseContentLength(const std::string& headers);
    
    void handleRequest(const std::string& route, int clientFd, const std::string& body, const std::unordered_map<std::string, std::string>& queryParams);
    
    void initRouteRequestBuilders();
    
    // Common request setup
    void prepareRequest(HttpRequest& request, const std::string& route);
    
    void sendBadRequest(int clientFd);
    void sendNotFound(int clientFd);
    
};
