#pragma once

#include "HttpRequest.hpp"

#include <curl/curl.h>

#include <string>

class HttpClient {
public:
    HttpClient();
    virtual ~HttpClient();

    HttpRequest createRequest(const std::string& url);

    virtual bool sendRequest(const HttpRequest& request, HttpRequest::Reply& reply, bool debug = false);
    virtual bool sendStreamingRequest(const HttpRequest& request, int outputFd, bool debug = false);

protected:
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t writeHeaderCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t writeCallbackStream(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t writeHeaderCallbackStream(char* ptr, size_t size, size_t nmemb, void* userdata);
    static int debugCallback(CURL* handle, curl_infotype type, char* data, size_t size, void* userptr);
private:
    CURLSH* m_shared;
};
