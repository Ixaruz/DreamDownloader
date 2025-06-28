#include <net/HttpClient.hpp>

#include <sstream>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>

namespace {
    struct StreamContext {
        int fd;
        bool headerSent;
        bool chunked;
        size_t contentLength;
        std::string contentType;
        bool connectionClosed; // Track connection state
        
        StreamContext(int socket_fd) 
            : fd(socket_fd), headerSent(false), chunked(false), 
            contentLength(0), contentType("application/octet-stream"),
            connectionClosed(false) {}
    };

    bool sendAll(int fd, const char* data, size_t len) {
        size_t sent = 0;
        int retryCount = 0;
        const int maxRetries = 100; // Prevent infinite loops
        
        while (sent < len && retryCount < maxRetries) {
            ssize_t result = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
            
            if (result == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Socket buffer full, use exponential backoff
                    retryCount++;
                    usleep(1000 * retryCount); // 1ms, 2ms, 3ms, etc.
                    continue;
                } else if (errno == EPIPE || errno == ECONNRESET) {
                    // Connection broken
                    return false;
                } else if (errno == EINTR) {
                    // Interrupted system call, retry immediately
                    continue;
                } else {
                    // Other serious error
                    return false;
                }
            } else if (result == 0) {
                // Connection closed by peer
                return false;
            }
            
            sent += result;
            retryCount = 0; // Reset retry count on successful send
        }
        
        return sent == len;
    }
    
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
        
        rawRequest << "\r\nBody:\r\n"; // End of headers
        
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
    m_shared = curl_share_init();
    curl_share_setopt(m_shared, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(m_shared, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);  // Share connections
    curl_share_setopt(m_shared, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);      // Share DNS cache
}

HttpClient::~HttpClient() {
    curl_share_cleanup(m_shared);
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

    // Enable connection sharing and keep-alive
    curl_easy_setopt(curl, CURLOPT_SHARE, m_shared);
    
    // HTTP keep-alive settings
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    
    // Connection reuse settings
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 0L);
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 0L);
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 10L);
    
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

bool HttpClient::sendStreamingRequest(const HttpRequest& request, int outputFd, bool debug) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    // Enable connection sharing and keep-alive
    curl_easy_setopt(curl, CURLOPT_SHARE, m_shared);
    
    // HTTP keep-alive settings
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    
    // Connection reuse settings
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 0L);
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 0L);
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 10L);
    
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
    
    // Custom write callback
    StreamContext context = StreamContext(outputFd);
    
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallbackStream);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
    
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeHeaderCallbackStream);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &context);
    
    CURLcode res = CURL_LAST;
    if (debug) {
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, HttpClient::debugCallback);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        const std::string headMessage = "HTTP/1.1 218 This is fine\r\nContent-Type: application/octet-stream\r\n";
        std::string rawBody = buildRawRequestDebugInfo(curl, request, headerList);
        std::stringstream msg;
        // two CRLF are important to distinguish HTTP head from body
        msg << headMessage << "Content-Length: " << rawBody.size() << "\r\n\r\n" << rawBody;
        send(outputFd, msg.str().c_str(), msg.str().size(), 0);
    }
    else {
        res = curl_easy_perform(curl);
        if (context.chunked) {
            send(context.fd, "0\r\n\r\n", 5, 0); // Final chunk
        }
    }
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

// Improved stream write callback with better error handling
size_t HttpClient::writeCallbackStream(char* ptr, size_t size, size_t nmemb, void* userdata) {
    StreamContext* context = static_cast<StreamContext*>(userdata);
    size_t total = size * nmemb;

    if (!context->headerSent) {
        // Buffer data until headers are sent
        return total;
    }

    if (context->chunked) {
        const size_t maxChunkSize = 8192; // Larger chunks for better performance
        size_t sent = 0;

        while (sent < total) {
            size_t chunkSize = std::min(maxChunkSize, total - sent);
            
            // Build chunk header
            char chunkHeader[16];
            int headerLen = snprintf(chunkHeader, sizeof(chunkHeader), "%x\r\n", (unsigned int)chunkSize);
            
            // Send chunk header
            if (!sendAll(context->fd, chunkHeader, headerLen)) {
                return 0; // Signal error to curl
            }
            
            // Send chunk data
            if (!sendAll(context->fd, ptr + sent, chunkSize)) {
                return 0;
            }
            
            // Send chunk trailer
            if (!sendAll(context->fd, "\r\n", 2)) {
                return 0;
            }
            
            sent += chunkSize;
        }
    } else {
        // Non-chunked transfer
        if (!sendAll(context->fd, ptr, total)) {
            return 0;
        }
    }

    return total;
}


// Improved header callback with better synchronization
size_t HttpClient::writeHeaderCallbackStream(char* buffer, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    StreamContext* context = static_cast<StreamContext*>(userdata);
    std::string line(buffer, total);

    // Remove trailing CRLF for processing
    if (line.size() >= 2 && line.substr(line.size() - 2) == "\r\n") {
        line = line.substr(0, line.size() - 2);
    }

    // Parse Content-Type header
    if (line.find("Content-Type:") == 0 || line.find("content-type:") == 0) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            context->contentType = line.substr(colonPos + 1);
            // Trim whitespace
            size_t start = context->contentType.find_first_not_of(" \t");
            if (start != std::string::npos) {
                context->contentType = context->contentType.substr(start);
            }
            size_t end = context->contentType.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) {
                context->contentType = context->contentType.substr(0, end + 1);
            }
        }
    }
    
    // Parse Content-Length header
    if (line.find("Content-Length:") == 0 || line.find("content-length:") == 0) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string lengthStr = line.substr(colonPos + 1);
            // Trim whitespace
            size_t start = lengthStr.find_first_not_of(" \t");
            if (start != std::string::npos) {
                lengthStr = lengthStr.substr(start);
            }
            try {
                context->contentLength = std::stoul(lengthStr);
            } catch (const std::exception&) {
                context->contentLength = 0;
            }
        }
    }
    
    // Check for end of headers (empty line)
    if (buffer[0] == '\r' && buffer[1] == '\n' && !context->headerSent) {
        // Send response headers
        std::ostringstream responseHeaders;
        responseHeaders << "HTTP/1.1 200 OK\r\n";
        responseHeaders << "Content-Type: " << context->contentType << "\r\n";
        
        if (context->contentLength > 0) {
            responseHeaders << "Content-Length: " << context->contentLength << "\r\n";
            context->chunked = false;
        } else {
            responseHeaders << "Transfer-Encoding: chunked\r\n";
            context->chunked = true;
        }
        
        // Add keep-alive headers if needed
        responseHeaders << "Connection: keep-alive\r\n";
        responseHeaders << "\r\n";
        
        std::string headerStr = responseHeaders.str();
        if (!sendAll(context->fd, headerStr.c_str(), headerStr.size())) {
            return 0; // Signal error
        }
        
        context->headerSent = true;
    }
    
    return total;
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
