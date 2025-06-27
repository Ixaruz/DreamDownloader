#pragma once

#include "HttpClient.hpp"

#include <switch/types.h>

#include <msgpack.hpp>

class AcbaaClient : public HttpClient {
public:
    explicit AcbaaClient(const std::string& bearerToken);
    
    // setting the requests headers and sending the request
    bool sendRequest(HttpRequest& request, HttpRequest::Reply& reply, bool debug = false);
    
    bool requestDreamLandsById(u64 dreamId, HttpRequest::Reply& reply, bool debug = false);
    
    bool requestDreamLands(const std::vector<std::pair<std::string, std::string>>& query, HttpRequest::Reply& reply, bool debug = false);
    bool sendFeedbackForDream(u64 dreamId, const std::string& feedbackBody, HttpRequest::MimeType mime, HttpRequest::Reply& reply, bool debug = false);

private:
    std::string m_bearerToken;
    std::string m_userAgent;
    std::string m_baseUrl;
};
