#pragma once

#include <switch/types.h>

#include <string>
#include <vector>
#include <utility>
#include <cstdint>

class HttpRequest {
public:
    enum class MimeType {
        None,
        Json,
        Msgpack,
        OctetStream
    };

    enum class HttpMethod {
        Get,
        Post,
        Put
    };
    
    typedef std::vector<std::pair<std::string, std::string>> HeaderFields;
    
    struct Reply
	{
		HeaderFields headers;
		std::string body;
		int responseCode;
	};

    HttpRequest();

    void setUrl(const std::string& u);
    std::string getUrl() const;

    void setMethod(HttpMethod m);
    HttpMethod getMethod() const;

    void setBody(const std::string& b);
    void setBody(const std::vector<u8>& data);
    std::string getBody() const;

    void setMimeType(MimeType type);
    void applyMimeType();
    MimeType getMimeType() const;

    void setHeader(const std::string& key, const std::string& value);
    static void setHeader(HeaderFields& headers, const std::string& key, const std::string& value);
    void setHeaders(const HeaderFields& newHeaders);
    const std::vector<std::pair<std::string, std::string>>& getHeaders() const;

    void setQueryParam(const std::string& key, const std::string& value);
    void setQueryParams(const std::vector<std::pair<std::string, std::string>>& params);
    const std::vector<std::pair<std::string, std::string>>& getQueryParams() const;

    std::string buildUrlWithParams() const;

    static std::string mimeTypeToString(MimeType mime);
    static std::string httpMethodToString(HttpMethod method);

private:
    std::string m_url;
    HeaderFields m_headers;
    std::vector<std::pair<std::string, std::string>> m_queryParams;
    std::string m_body;
    MimeType m_mime;
    HttpMethod m_method;
};
