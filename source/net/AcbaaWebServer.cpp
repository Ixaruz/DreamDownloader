#include <net/AcbaaWebServer.hpp>

#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <poll.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

namespace {
    // https://www.geeksforgeeks.org/cpp/how-to-split-cpp-string-into-vector-of-substrings/
    std::vector<std::string> splitString(std::string& input, char delimiter)
    {
    
        // Creating an input string stream from the input string
        std::istringstream stream(input);
    
        // Vector to store the tokens
        std::vector<std::string> tokens;
    
        // Temporary string to store each token
        std::string token;
    
        // Read tokens from the string stream separated by the
        // delimiter
        while (getline(stream, token, delimiter)) {
            // Add the token to the vector of tokens
            tokens.push_back(token);
        }
    
        // Return the vector of tokens
        return tokens;
    }
    
    void extractQueryParams(std::string& uri, std::unordered_map<std::string, std::string>& queryParams) {
        // Parse query parameters if any
        size_t qPos = uri.find('?');
        if (qPos != std::string::npos) {
            std::string query = uri.substr(qPos + 1);

            size_t pos = 0;
            while (pos < query.length()) {
                size_t amp = query.find('&', pos);
                std::string pair = query.substr(pos, amp - pos);
                size_t eq = pair.find('=');
                if (eq != std::string::npos) {
                    queryParams[pair.substr(0, eq)] = pair.substr(eq + 1);
                }
                if (amp == std::string::npos) break;
                pos = amp + 1;
            }

            uri = uri.substr(0, qPos);
        }
    }
    
    void extractQueryParams(std::string& uri, std::vector<std::pair <std::string, std::string>>& queryParams) {
        // Parse query parameters if any
        size_t qPos = uri.find('?');
        if (qPos != std::string::npos) {
            std::string query = uri.substr(qPos + 1);

            size_t pos = 0;
            while (pos < query.length()) {
                size_t amp = query.find('&', pos);
                std::string pair = query.substr(pos, amp - pos);
                size_t eq = pair.find('=');
                if (eq != std::string::npos) {
                    queryParams.emplace_back(pair.substr(0, eq), pair.substr(eq + 1));
                }
                if (amp == std::string::npos) break;
                pos = amp + 1;
            }

            uri = uri.substr(0, qPos);
        }
    }

    
    bool setNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        return (flags != -1) && (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);
    }
}

AcbaaWebServer::AcbaaWebServer(const std::string& bearerToken)
    : m_serverFd(-1),
      m_running(false),
      m_bearerToken(bearerToken),
      m_userAgent("libcurl/7.64.1 (HAC; nnEns; SDK 20.5.4.0)"),
      m_baseUrl("https://api.hac.lp1.acbaa.srv.nintendo.net") {
    initRouteRequestBuilders();
}

AcbaaWebServer::~AcbaaWebServer() {
}

bool AcbaaWebServer::start(u16 port) {
    // Construct a socket address where we want to listen for requests
    static struct sockaddr_in serv_addr;
    serv_addr.sin_addr.s_addr = INADDR_ANY; // The Switch'es IP address
    serv_addr.sin_port = htons(port);
    serv_addr.sin_family = PF_INET; // The Switch only supports AF_INET and AF_ROUTE: https://switchbrew.org/wiki/Sockets_services#Socket
    
    m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd < 0) return false;
    if (!setNonBlocking(m_serverFd)) return false;

        // Set a relatively short timeout for recv() calls, see CWebServer::ServeRequest for more info why
    struct timeval recvTimeout;
    recvTimeout.tv_sec = 1;
    recvTimeout.tv_usec = 0;
    setsockopt(m_serverFd, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout));
    
    int opt = 1;
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    // sockaddr_in serverAddr{};
    // serverAddr.sin_family = AF_INET;
    // serverAddr.sin_addr.s_addr = INADDR_ANY;
    // serverAddr.sin_port = htons(port);
    
    // Bind the just-created socket to the address
    if (bind(m_serverFd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) return false;
    // Start listening to the socket with 5 maximum parallel connections
    if (listen(m_serverFd, 10) < 0) return false;

    m_running = true;
    return true;
}

bool AcbaaWebServer::serverLoop() {
    if (m_serverFd < 0)
        return false;

    pollfd pfd = { .fd = m_serverFd, .events = POLLIN, .revents = 0 };

    // Non-blocking poll with no timeout
    int ret = poll(&pfd, 1, 0);
    if (ret < 0) {
        perror("poll");
        return false;
    }
    if (ret == 0) {
        // no incoming connection this iteration
        return true;
    }

    // Incoming connection ready
    if (pfd.revents & POLLIN) {
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        int clientFd = accept(m_serverFd, (sockaddr*)&clientAddr, &addrLen);
        if (clientFd < 0) {
            perror("accept");
            return true;
        }
        
        printf("Accepted connection from %s:%u\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        // Handle the entire request + streaming in one shot
        handleClient(clientFd);

        close(clientFd);
        printf("Closed connection (fd=%d)\n", clientFd);
    }

    return true;
}

std::tuple<std::string,std::string,std::string,std::unordered_map<std::string,std::string>>
AcbaaWebServer::parseHttpRequest(const std::string& fullReq) {
    // 1) Find end of headers
    auto hdrEnd = fullReq.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) {
        throw std::runtime_error("Malformed request: no header terminator");
    }

    // 2) Extract headers block
    std::string headers = fullReq.substr(0, hdrEnd);
    std::string postBody;
    size_t bodyLen = 0;

    // 3) Parse Content-Length, if any
    {
        std::istringstream hs(headers);
        std::string line;
        while (std::getline(hs, line)) {
            if (line.size() >= 15 && 
                std::equal(line.begin(), line.begin()+15, "Content-Length:")) {
                bodyLen = std::stoul(line.substr(15));
                break;
            }
        }
    }

    // 4) If there's a body, extract it
    if (bodyLen > 0) {
        size_t bodyStart = hdrEnd + 4;
        if (fullReq.size() < bodyStart + bodyLen)
            throw std::runtime_error("Malformed request: body shorter than Content-Length");
        postBody = fullReq.substr(bodyStart, bodyLen);
    }

    // 5) Parse request-line (first line of headers)
    auto lineEnd = headers.find("\r\n");
    std::string requestLine = headers.substr(0, lineEnd);
    auto parts = splitString(requestLine, ' ');
    if (parts.size() != 3)
        throw std::runtime_error("Malformed request-line");

    std::string method = parts[0];
    std::string uri    = parts[1];

    // 6) Extract query parameters from URI
    std::unordered_map<std::string,std::string> queryParams;
    auto qPos = uri.find('?');
    if (qPos != std::string::npos) {
        std::string qs = uri.substr(qPos+1);
        uri = uri.substr(0, qPos);

        size_t pos = 0;
        while (pos < qs.size()) {
            auto amp = qs.find('&', pos);
            auto pair = qs.substr(pos, amp - pos);
            auto eq = pair.find('=');
            if (eq != std::string::npos) {
                auto key = pair.substr(0, eq);
                auto val = pair.substr(eq+1);
                queryParams[key] = val;
            }
            if (amp == std::string::npos) break;
            pos = amp + 1;
        }
    }

    return { method, uri, postBody, queryParams };
}

size_t AcbaaWebServer::parseContentLength(const std::string& headers) {
    std::istringstream ss(headers);
    std::string line;
    const std::string key = "Content-Length:";
    while (std::getline(ss, line)) {
        if (line.size() >= key.size() &&
            std::equal(key.begin(), key.end(), line.begin(),
                       [](char a, char b){ return std::tolower(a)==std::tolower(b); }))
        {
            // skip past the header name and any whitespace
            auto val = line.substr(key.size());
            size_t pos = val.find_first_not_of(" \t");
            if (pos != std::string::npos) val = val.substr(pos);
            return std::stoul(val);
        }
    }
    return 0;
}

void AcbaaWebServer::handleClient(int clientFd) {
    // 1) Read headers + body fully (as you already do with Content-Length)
    std::string fullReq;
    while (true) {
        char buf[512];
        ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
        if (n <= 0) break;  // EOF or error
        fullReq.append(buf, n);
        if (fullReq.find("\r\n\r\n") != std::string::npos) {
            // parse Content-Length, wait until full body is in fullReq...
            size_t hdrEnd = fullReq.find("\r\n\r\n");
            size_t bodyLen = parseContentLength(fullReq.substr(0, hdrEnd));
            if (fullReq.size() >= hdrEnd + 4 + bodyLen) break;
        }
    }

    // 2) Parse out method, URI, postBody, queryParams...
    auto [method, uri, postBody, queryParams] = parseHttpRequest(fullReq);

    // 3) Dispatch to your existing handleRequest (which does the libcurl streaming)
    handleRequest(uri, clientFd, postBody, queryParams);

    // 4) Done: sendStreamingRequest will have written all bytes (blocking),
    //    so when we return, it's safe to close() the socket.
}

void AcbaaWebServer::handleRequest(
    const std::string& route,
    int clientFd,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& queryParams) {
    auto routeIt = m_routeRequestBuilders.find(route);
    if (routeIt == m_routeRequestBuilders.end()) {
        sendNotFound(clientFd);
        return;
    }

    const auto& paramMap = routeIt->second;
    for (const auto& [paramKey, builder] : paramMap) {
        // For requests without params, we can define an empty string in m_routeRequestBuilders.
        // TODO: find out if this is a bad way to set keys in hash maps.. (hash = 0)
        if (paramKey.empty() || 0 == paramKey.compare("default") || 
            queryParams.find(paramKey) != queryParams.end()) {
            std::optional<HttpRequest> maybeRequest;
            try {
                maybeRequest = builder(body, queryParams);
                if(!maybeRequest.has_value()) {
                    sendBadRequest(clientFd);
                    return;
                }
            }
            catch(...) {
                sendBadRequest(clientFd);
                return;
            }
            HttpRequest request = maybeRequest.value();
            prepareRequest(request, route);
            sendStreamingRequest(request, clientFd, /*debug*/ false);
            return;
        }
    }

    sendBadRequest(clientFd);
}

void AcbaaWebServer::initRouteRequestBuilders() {
    // Builders for /dream_lands
    m_routeRequestBuilders["/dream_query"] = {
        {"id", [this](const std::string& body, const auto& params) -> std::optional<HttpRequest> {
            HttpRequest req;
            req.setMethod(HttpRequest::HttpMethod::Get);
            req.setUrl(m_baseUrl + "/api/v1/dream_lands");
            req.setMimeType(HttpRequest::MimeType::Msgpack);
            req.setQueryParams({
                {"offset", "0"},
                {"limit", "150"},
                {"q[id]", params.at("id")}
            });
            return req;
        }},
        {"land_name", [this](const std::string& body, const auto& params) -> std::optional<HttpRequest> {
            HttpRequest req;
            req.setMethod(HttpRequest::HttpMethod::Get);
            req.setUrl(m_baseUrl + "/api/v1/dream_lands");
            req.setMimeType(HttpRequest::MimeType::Msgpack);
            req.setQueryParams({
                {"offset", "0"},
                {"limit", "150"},
                {"q[search_type]", "name"},
                {"q[land_name]", params.at("land_name")}
            });
            return req;
        }},
        {"recommend", [this](const std::string& body, const auto& params) -> std::optional<HttpRequest> {
            if (params.end() == params.find("lang")) {
                return std::nullopt;
            }
            HttpRequest req;
            req.setMethod(HttpRequest::HttpMethod::Get);
            req.setUrl(m_baseUrl + "/api/v1/dream_lands");
            req.setMimeType(HttpRequest::MimeType::Msgpack);
            req.setQueryParams({
                {"offset", "0"},
                {"limit", "150"},
                {"q[search_type]", "recommend"},
                {"q[lang]", params.at("lang")}
            });
            return req;
        }}
    };
    
    // Builders for /dream_land/
    m_routeRequestBuilders["/dream_download"] = {
        {"default", [this](const std::string& body, const auto& params) -> std::optional<HttpRequest> {
            if (body.empty()) {
                return std::nullopt;
            }
            std::string url = body;
            // m_baseUrl has to be used. I know this can be circumvented by sending requests to 
            // $m_baseUrl.mySubdomain.wowMomImAHacker.org, but at that point, I don't care enough
            // about implementing security checks.
            if (0 != url.rfind(m_baseUrl, 0)) {
                return std::nullopt; 
            }
            std::vector<std::pair <std::string, std::string>> dreamDownloadQuery;
            extractQueryParams(url, dreamDownloadQuery);
            HttpRequest req;
            req.setMethod(HttpRequest::HttpMethod::Get);
            req.setUrl(url);
            req.setQueryParams(dreamDownloadQuery);
            return req;
        }}
    };
    
    // exempt /dream_download requests to include Authorization: Bearer ...,
    // when sending a request.
    // this probably isn't as elegant, if I want to use this endpoint for custom requests,
    // but it will suffice for now.
    m_routeAuthorizationExemptions["/dream_download"] = true;
    
    // Builders for /friend_requests
    m_routeRequestBuilders["/friend_requests"] = {
        {"type", [this](const std::string& body, const auto& params) -> std::optional<HttpRequest> {
            if ("receive" != params.at("type") && "send" != params.at("type")) {
                return std::nullopt; 
            }
            HttpRequest req;
            req.setMethod(HttpRequest::HttpMethod::Get);
            req.setUrl(m_baseUrl + "/api/v1/friend_requests");
            req.setQueryParams({
                {"type", params.at("type")},
            });
            return req;
        }},
    };
    
}

void AcbaaWebServer::prepareRequest(HttpRequest& request, const std::string& route) {
    request.setHeader("User-Agent", m_userAgent);
    request.setHeader("Accept", "*/*");
    if (m_routeAuthorizationExemptions.end() == m_routeAuthorizationExemptions.find(route) ||
       !m_routeAuthorizationExemptions.at(route)) {
            request.setHeader("Authorization", "Bearer " + m_bearerToken);
        }
    request.applyMimeType();
}

void AcbaaWebServer::sendBadRequest(int clientFd) {
    const std::string msg = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
    send(clientFd, msg.c_str(), msg.size(), 0);
}

void AcbaaWebServer::sendNotFound(int clientFd) {
    const std::string msg = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    send(clientFd, msg.c_str(), msg.size(), 0);
}
