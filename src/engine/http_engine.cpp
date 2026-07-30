#include "engine/http_engine.h"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace remo {
namespace engine {

namespace {

size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* stream = static_cast<std::ofstream*>(userp);
    const size_t totalSize = size * nmemb;
    stream->write(static_cast<char*>(contents), static_cast<std::streamsize>(totalSize));
    return totalSize;
}

size_t headerCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* headers = static_cast<std::vector<std::string>*>(userp);
    const size_t totalSize = size * nmemb;
    headers->emplace_back(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

} // namespace

HttpEngine::HttpEngine() = default;

HttpEngine::~HttpEngine() = default;

bool HttpEngine::sendHeadRequest(const std::string& url, int64_t& fileSize, std::string& etag, std::string& lastModified) {
    return performHeadRequest(url, fileSize, etag, lastModified);
}

bool HttpEngine::supportsRangeRequests(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    std::vector<std::string> headers;
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_cleanup(curl);
    for (const auto& header : headers) {
        const std::string lower = lowerCopy(header);
        if (lower.rfind("accept-ranges:", 0) == 0 && lower.find("bytes") != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool HttpEngine::downloadSegment(const std::string& url, int64_t startByte, int64_t endByte, const std::string& outputPath) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    std::string rangeValue;
    if (endByte > startByte) {
        rangeValue = std::to_string(startByte) + "-" + std::to_string(endByte);
        curl_easy_setopt(curl, CURLOPT_RANGE, rangeValue.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);

    file.close();
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}

bool HttpEngine::performHeadRequest(const std::string& url, int64_t& fileSize, std::string& etag, std::string& lastModified) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);

    std::vector<std::string> headers;
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        long responseCode = 0;
        curl_off_t contentLength = -1;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);
        fileSize = contentLength > 0 ? static_cast<int64_t>(contentLength) : 0;

        for (const auto& header : headers) {
            if (header.rfind("ETag:", 0) == 0) {
                etag = header.substr(6);
                etag.erase(0, etag.find_first_not_of(" \t"));
                etag.erase(etag.find_last_not_of(" \r\n") + 1);
            } else if (header.rfind("Last-Modified:", 0) == 0) {
                lastModified = header.substr(14);
                lastModified.erase(0, lastModified.find_first_not_of(" \t"));
                lastModified.erase(lastModified.find_last_not_of(" \r\n") + 1);
            }
        }
    }

    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

} // namespace engine
} // namespace remo
