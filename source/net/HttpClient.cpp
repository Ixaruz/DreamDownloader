#include <net/HttpClient.hpp>

#include <sstream>
#include <algorithm>

namespace {
    std::string buildRawRequestDebugInfo(CURL* curl, const HttpRequest& request, struct curl_slist* headerList) {
        std::ostringstream rawRequest;
        
        // Get method
        std::string method = HttpRequest::httpMethodToString(request.getMethod());
        
        // Get HTTP version
        long httpVersion = 0;
        curl_easy_getinfo(curl, CURLINFO_HTTP_VERSION, &httpVersion);
        
        std::string httpVersionStr = "HTTP/1.1"; // default fallback
        switch (httpVersion) {
            case CURL_HTTP_VERSION_1_0:
                httpVersionStr = "HTTP/1.0";
                break;
            case CURL_HTTP_VERSION_1_1:
                httpVersionStr = "HTTP/1.1";
                break;
            case CURL_HTTP_VERSION_2_0:
                httpVersionStr = "HTTP/2.0";
                break;
            case CURL_HTTP_VERSION_2TLS:
                httpVersionStr = "HTTP/2.0";
                break;
            case CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE:
                httpVersionStr = "HTTP/2.0";
                break;
            case CURL_HTTP_VERSION_3:
                httpVersionStr = "HTTP/3.0";
                break;
            default:
                httpVersionStr = "HTTP/1.1";
                break;
        }
    
        // Get URL components
        char* url = nullptr;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);
        
        // Parse URL to extract path and query
        std::string effectiveUrl = url ? url : "";
        std::string path = "/";
        std::string query = "";
        std::string host = "";
        
        // Simple URL parsing
        if (effectiveUrl.find("://") != std::string::npos) {
            size_t schemeEnd = effectiveUrl.find("://") + 3;
            size_t pathStart = effectiveUrl.find("/", schemeEnd);
            if (pathStart != std::string::npos) {
                host = effectiveUrl.substr(schemeEnd, pathStart - schemeEnd);
                std::string pathAndQuery = effectiveUrl.substr(pathStart);
                size_t queryStart = pathAndQuery.find("?");
                if (queryStart != std::string::npos) {
                    path = pathAndQuery.substr(0, queryStart);
                    query = pathAndQuery.substr(queryStart);
                } else {
                    path = pathAndQuery;
                }
            } else {
                host = effectiveUrl.substr(schemeEnd);
            }
        }
        
        // Build raw HTTP request
        rawRequest << method << " " << path << query << " " << httpVersionStr << "\r\n";
        rawRequest << "Host: " << host << "\r\n";
        
        // Add headers from curl_slist (the actual headers sent by curl)
        struct curl_slist* current = headerList;
        while (current) {
            rawRequest << current->data << "\r\n";
            current = current->next;
        }
        
        // Add default headers that curl might add automatically if not present in headerList
        bool hasUserAgent = false;
        bool hasContentLength = false;
        bool hasContentType = false;
        
        // Check what headers are already present
        current = headerList;
        while (current) {
            std::string header = current->data;
            std::string headerLower = header;
            std::transform(headerLower.begin(), headerLower.end(), headerLower.begin(), ::tolower);
            
            if (headerLower.find("user-agent:") == 0) hasUserAgent = true;
            if (headerLower.find("content-length:") == 0) hasContentLength = true;
            if (headerLower.find("content-type:") == 0) hasContentType = true;
            
            current = current->next;
        }
        
        // Add User-Agent if not already present
        if (!hasUserAgent) {
            rawRequest << "User-Agent: curl/libcurl\r\n";
        }
        
        // Add Content-Length for POST/PUT requests if not already present
        if (!hasContentLength && 
            (request.getMethod() == HttpRequest::HttpMethod::Post || 
            request.getMethod() == HttpRequest::HttpMethod::Put)) {
            rawRequest << "Content-Length: " << request.getBody().size() << "\r\n";
        }
        
        rawRequest << "\r\n"; // End of headers
        
        // Add body for POST/PUT requests
        if ((request.getMethod() == HttpRequest::HttpMethod::Post || 
            request.getMethod() == HttpRequest::HttpMethod::Put) && 
            !request.getBody().empty()) {
            rawRequest << request.getBody();
        }
        
        // Additional debug information
        rawRequest << "\r\n\r\n--- CURL DEBUG INFO ---\r\n";
        
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        rawRequest << "Response Code: " << responseCode << "\r\n";
        
        double totalTime = 0;
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &totalTime);
        rawRequest << "Total Time: " << totalTime << "s\r\n";
        
        double connectTime = 0;
        curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &connectTime);
        rawRequest << "Connect Time: " << connectTime << "s\r\n";
        
        long headerSize = 0;
        curl_easy_getinfo(curl, CURLINFO_HEADER_SIZE, &headerSize);
        rawRequest << "Header Size: " << headerSize << " bytes\r\n";
        
        long requestSize = 0;
        curl_easy_getinfo(curl, CURLINFO_REQUEST_SIZE, &requestSize);
        rawRequest << "Request Size: " << requestSize << " bytes\r\n";
        
        double downloadSize = 0;
        curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &downloadSize);
        rawRequest << "Download Size: " << downloadSize << " bytes\r\n";
        
        double uploadSize = 0;
        curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD, &uploadSize);
        rawRequest << "Upload Size: " << uploadSize << " bytes\r\n";
        
        char* primaryIp = nullptr;
        curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &primaryIp);
        if (primaryIp) {
            rawRequest << "Primary IP: " << primaryIp << "\r\n";
        }
        
        long primaryPort = 0;
        curl_easy_getinfo(curl, CURLINFO_PRIMARY_PORT, &primaryPort);
        rawRequest << "Primary Port: " << primaryPort << "\r\n";
        
        return rawRequest.str();
    }
}

HttpClient::HttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

HttpRequest HttpClient::createRequest(const std::string& url) {
    HttpRequest req;
    req.setUrl(url);
    return req;
}

bool HttpClient::sendRequest(const HttpRequest& request, HttpRequest::Reply& reply, bool debug) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    reply.responseCode = 0;
    
    std::string fullUrl = request.buildUrlWithParams();
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());

    struct curl_slist* headerList = nullptr;
    for (const auto& [key, value] : request.getHeaders()) {
        std::string h = key + ": " + value;
        headerList = curl_slist_append(headerList, h.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

    if (request.getMethod() == HttpRequest::HttpMethod::Post || request.getMethod() == HttpRequest::HttpMethod::Put) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.getBody().c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.getBody().size());
        if (request.getMethod() == HttpRequest::HttpMethod::Put) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        }
    } else if (request.getMethod() == HttpRequest::HttpMethod::Get) {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpClient::writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply.body);

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HttpClient::writeHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &reply.headers);
    
    CURLcode res = CURL_LAST;
    if (debug) {
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, HttpClient::debugCallback);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        reply.body = buildRawRequestDebugInfo(curl, request, headerList);
    }
    else {
        res = curl_easy_perform(curl);
    }
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply.responseCode);
    
    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK);
}

size_t HttpClient::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* str = static_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t HttpClient::writeHeaderCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    HttpRequest::HeaderFields* headers = static_cast<HttpRequest::HeaderFields*>(userdata);
    std::string line(ptr, size * nmemb);
    size_t split = line.find(':');
    if (split != std::string::npos)
    {
        HttpRequest::setHeader(*headers, std::string(line.substr(0, split)), std::string(line.substr(split + 1,  line.size() - split - 1)));
    }
    return size * nmemb;
}

int HttpClient::debugCallback(CURL* handle, curl_infotype type, char* data, size_t size, void* userptr) {
    (void)handle; (void)userptr;
    switch (type) {
        case CURLINFO_TEXT:
        case CURLINFO_HEADER_IN:
        case CURLINFO_HEADER_OUT:
        case CURLINFO_DATA_IN:
        case CURLINFO_DATA_OUT:
            printf("%.*s", (int)size, data);
            break;
        default:
            break;
    }
    return 0;
}
