#include "engine/http_engine.h"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <vector>

namespace remo {
namespace engine {

namespace {

// WriteCallback: writes received bytes to an ofstream
size_t writeToFileCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* stream = static_cast<std::ofstream*>(userp);
    const size_t totalSize = size * nmemb;
    stream->write(static_cast<char*>(contents), static_cast<std::streamsize>(totalSize));
    return stream->good() ? totalSize : 0;
}

// ProgressContext: carries progressCb + running byte count for downloadSegment
struct ProgressContext {
    std::function<bool(int64_t)> progressCb;
    int64_t downloaded = 0;
    bool aborted = false;
};

// CURLOPT_XFERINFOFUNCTION callback
int xferInfoCallback(void* clientp, curl_off_t /*dltotal*/, curl_off_t dlnow,
                     curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    if (!clientp) return 0;
    auto* ctx = static_cast<ProgressContext*>(clientp);
    if (dlnow > ctx->downloaded) {
        ctx->downloaded = static_cast<int64_t>(dlnow);
        if (ctx->progressCb && !ctx->progressCb(ctx->downloaded)) {
            ctx->aborted = true;
            return 1; // abort transfer
        }
    }
    return 0;
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

// Legacy overload (no errorMessage out-param)
bool HttpEngine::sendHeadRequest(const std::string& url, int64_t& fileSize,
                                 std::string& etag, std::string& lastModified) {
    std::string finalUrl;
    std::string errorMessage;
    return performHeadRequest(url, fileSize, etag, lastModified, finalUrl, errorMessage);
}

// Legacy overload (no errorMessage out-param)
bool HttpEngine::sendHeadRequest(const std::string& url, int64_t& fileSize,
                                 std::string& etag, std::string& lastModified,
                                 std::string& finalUrl) {
    std::string errorMessage;
    return performHeadRequest(url, fileSize, etag, lastModified, finalUrl, errorMessage);
}

// New overload with errorMessage
bool HttpEngine::sendHeadRequest(const std::string& url, int64_t& fileSize,
                                 std::string& etag, std::string& lastModified,
                                 std::string& finalUrl, std::string& errorMessage) {
    return performHeadRequest(url, fileSize, etag, lastModified, finalUrl, errorMessage);
}

void setupCurlDefaults(CURL* curl) {
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) RemooDownload/0.1");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
#ifdef _WIN32
#ifdef CURLSSLOPT_NATIVE_CA
    // Use Windows Native Certificate Store for SSL CA verification on MinGW/Windows
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
#endif
}

bool HttpEngine::supportsRangeRequests(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    char errbuf[CURL_ERROR_SIZE] = {0};
    std::vector<std::string> headers;

    setupCurlDefaults(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return false;

    for (const auto& header : headers) {
        const std::string lower = lowerCopy(header);
        if (lower.rfind("accept-ranges:", 0) == 0 && lower.find("bytes") != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Legacy downloadSegment (no progressCb, no errorMessage)
bool HttpEngine::downloadSegment(const std::string& url, int64_t startByte,
                                 int64_t endByte, const std::string& outputPath) {
    std::string errorMessage;
    return downloadSegment(url, startByte, endByte, outputPath, nullptr, errorMessage);
}

// New downloadSegment with progressCb and errorMessage
bool HttpEngine::downloadSegment(const std::string& url, int64_t startByte, int64_t endByte,
                                 const std::string& outputPath,
                                 std::function<bool(int64_t)> progressCb,
                                 std::string& errorMessage) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        errorMessage = "curl_easy_init() failed";
        return false;
    }

    std::ofstream file(outputPath, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        curl_easy_cleanup(curl);
        errorMessage = "Cannot open output file: " + outputPath;
        return false;
    }

    char errbuf[CURL_ERROR_SIZE] = {0};
    ProgressContext pctx;
    pctx.progressCb = progressCb;

    setupCurlDefaults(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    // Range request
    std::string rangeValue;
    if (endByte > startByte) {
        rangeValue = std::to_string(startByte) + "-" + std::to_string(endByte);
        curl_easy_setopt(curl, CURLOPT_RANGE, rangeValue.c_str());
    }

    // Progress reporting
    if (progressCb) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfoCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pctx);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    file.close();

    if (res != CURLE_OK) {
        errorMessage = "curl_easy_perform failed [code " + std::to_string(res) + "]: " +
                       std::string(curl_easy_strerror(res));
        if (errbuf[0]) errorMessage += " (" + std::string(errbuf) + ")";
        curl_easy_cleanup(curl);
        return false;
    }

    if (pctx.aborted) {
        errorMessage = "Transfer aborted by progressCb";
        curl_easy_cleanup(curl);
        return false;
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (httpCode >= 400) {
        errorMessage = "HTTP error: " + std::to_string(httpCode);
        return false;
    }

    return true;
}

bool HttpEngine::performHeadRequest(const std::string& url,
                                    int64_t& fileSize,
                                    std::string& etag,
                                    std::string& lastModified,
                                    std::string& finalUrl,
                                    std::string& errorMessage) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        errorMessage = "curl_easy_init() failed";
        return false;
    }

    char errbuf[CURL_ERROR_SIZE] = {0};
    std::vector<std::string> headers;

    setupCurlDefaults(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        errorMessage = "curl_easy_perform failed [code " + std::to_string(res) + "]: " +
                       std::string(curl_easy_strerror(res));
        if (errbuf[0]) errorMessage += " (" + std::string(errbuf) + ")";
        curl_easy_cleanup(curl);
        return false;
    }

    long httpCode = 0;
    curl_off_t contentLength = -1;
    char* effectiveUrl = nullptr;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl);

    if (httpCode >= 400) {
        errorMessage = "HTTP error: " + std::to_string(httpCode);
        curl_easy_cleanup(curl);
        return false;
    }

    fileSize = contentLength > 0 ? static_cast<int64_t>(contentLength) : 0;
    finalUrl = effectiveUrl != nullptr ? effectiveUrl : url;

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

    curl_easy_cleanup(curl);
    return true;
}

} // namespace engine
} // namespace remo
