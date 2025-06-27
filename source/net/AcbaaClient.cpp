#include <net/AcbaaClient.hpp>

AcbaaClient::AcbaaClient(const std::string& bearerToken)
    : m_bearerToken(bearerToken),
      m_userAgent("libcurl/7.64.1 (HAC; nnEns; SDK 10.9.8.0)"),
      m_baseUrl("https://api.hac.lp1.acbaa.srv.nintendo.net"),
      HttpClient::HttpClient() {}


bool AcbaaClient::sendRequest(HttpRequest& request, HttpRequest::Reply& reply, bool debug) {
    // don't overwrite if possible
    request.setHeader("User-Agent", m_userAgent);
    request.setHeader("Accept", "*/*");
    request.setHeader("Authorization", "Bearer " + m_bearerToken);
    request.setMimeType(HttpRequest::MimeType::Msgpack);
    request.applyMimeType();
    
    return HttpClient::sendRequest(request, reply, debug);
}
    
bool AcbaaClient::requestDreamLandsById(u64 dreamId, HttpRequest::Reply& reply, bool debug) {
    const std::vector<std::pair<std::string, std::string>> query = {
        {"offset", "0"},
        {"limit", "150"},
        {"q[id]", std::to_string(dreamId)}
    };
    
    return requestDreamLands(query, reply, debug);
}
    
bool AcbaaClient::requestDreamLands(const std::vector<std::pair<std::string, std::string>>& query, HttpRequest::Reply& reply, bool debug) {
    HttpRequest req = createRequest(m_baseUrl + "/api/v1/dream_lands");
    req.setMethod(HttpRequest::HttpMethod::Get);
    req.setQueryParams(query);

    return AcbaaClient::sendRequest(req, reply, debug);
}

bool AcbaaClient::sendFeedbackForDream(uint64_t dreamId, const std::string& feedbackBody, HttpRequest::MimeType mime, HttpRequest::Reply& reply, bool debug) {
    HttpRequest req = createRequest(m_baseUrl + "/api/v1/dream_lands/" + std::to_string(dreamId) + "/feedback");
    req.setMethod(HttpRequest::HttpMethod::Post);
    req.setBody(feedbackBody);
    req.setMimeType(mime);

    return AcbaaClient::sendRequest(req, reply, debug);
}
