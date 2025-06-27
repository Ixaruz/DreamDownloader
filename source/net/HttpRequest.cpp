#include <net/HttpRequest.hpp>

#include <curl/curl.h>

#include <sstream>

HttpRequest::HttpRequest()
    : m_mime(MimeType::None), m_method(HttpMethod::Get) {}

void HttpRequest::setUrl(const std::string& u) { m_url = u; }
std::string HttpRequest::getUrl() const { return m_url; }

void HttpRequest::setMethod(HttpMethod m) { m_method = m; }
HttpRequest::HttpMethod HttpRequest::getMethod() const { return m_method; }

void HttpRequest::setBody(const std::string& b) { m_body = b; }
void HttpRequest::setBody(const std::vector<uint8_t>& data) {
    m_body.assign(reinterpret_cast<const char*>(data.data()), data.size());
}
std::string HttpRequest::getBody() const { return m_body; }

void HttpRequest::setMimeType(MimeType type) {
    m_mime = type;
}

void HttpRequest::applyMimeType() {
    setHeader("Content-Type", mimeTypeToString(m_mime));
}

HttpRequest::MimeType HttpRequest::getMimeType() const { return m_mime; }

void HttpRequest::setHeader(HeaderFields& headers, const std::string& key, const std::string& value) {
    for (auto& pair : headers) {
        if (pair.first == key) {
            pair.second = value;
            return;
        }
    }
    headers.emplace_back(key, value);
}

void HttpRequest::setHeader(const std::string& key, const std::string& value) {
    HttpRequest::setHeader(m_headers, key, value);
}

void HttpRequest::setHeaders(const HeaderFields& newHeaders) {
    m_headers = newHeaders;
}

const HttpRequest::HeaderFields& HttpRequest::getHeaders() const {
    return m_headers;
}

void HttpRequest::setQueryParam(const std::string& key, const std::string& value) {
    for (auto& pair : m_queryParams) {
        if (pair.first == key) {
            pair.second = value;
            return;
        }
    }
    m_queryParams.emplace_back(key, value);
}

void HttpRequest::setQueryParams(const std::vector<std::pair<std::string, std::string>>& params) {
    m_queryParams = params;
}

const std::vector<std::pair<std::string, std::string>>& HttpRequest::getQueryParams() const {
    return m_queryParams;
}

std::string HttpRequest::buildUrlWithParams() const {
    if (m_queryParams.empty()) return m_url;

    std::ostringstream oss;
    oss << m_url << "?";
    bool first = true;
    for (const auto& [k, v] : m_queryParams) {
        if (!first) oss << "&";
        char* ek = curl_easy_escape(nullptr, k.c_str(), 0);
        char* ev = curl_easy_escape(nullptr, v.c_str(), 0);
        oss << ek << "=" << ev;
        curl_free(ek);
        curl_free(ev);
        first = false;
    }
    return oss.str();
}

std::string HttpRequest::mimeTypeToString(MimeType mime) {
    switch (mime) {
        case MimeType::Json: return "application/json";
        case MimeType::Msgpack: return "application/x-msgpack";
        case MimeType::OctetStream: return "application/octet-stream";
        default: return "application/octet-stream";
    }
}

std::string HttpRequest::httpMethodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::Get: return "GET";
        case HttpMethod::Post: return "POST";
        case HttpMethod::Put: return "PUT";
        default: return "GET";
    }
}
