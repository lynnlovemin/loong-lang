#ifndef LOONG_HTTP_HPP
#define LOONG_HTTP_HPP

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <wininet.h>
    #pragma comment(lib, "wininet.lib")
    #include <cstdio>
    #include <memory>
    #include <array>
#endif

#include <string>
#include <map>
#include <sstream>

namespace loong {

// HTTP响应结构
struct HttpResponse {
    int statusCode;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string error;
    bool success;
};

// HTTP请求类
class HttpClient {
public:
    // GET请求
    static HttpResponse get(const std::string& url, 
                           const std::map<std::string, std::string>& headers = {}) {
        return request(url, "GET", "", headers);
    }
    
    // POST请求
    static HttpResponse post(const std::string& url, 
                            const std::string& body,
                            const std::map<std::string, std::string>& headers = {}) {
        return request(url, "POST", body, headers);
    }
    
    // 通用请求
    static HttpResponse request(const std::string& url,
                               const std::string& method,
                               const std::string& body = "",
                               const std::map<std::string, std::string>& headers = {}) {
        HttpResponse response;
        response.success = false;
        response.statusCode = 0;
        
#ifdef _WIN32
        // 检查是否为HTTPS - 如果是，使用curl.exe
        bool isHttps = (url.length() > 8 && url.substr(0, 8) == "https://");
        
        if (isHttps) {
            // 使用curl.exe处理HTTPS请求（更可靠）
            return requestWithCurl(url, method, body, headers);
        }
        
        // HTTP请求使用WinINet
        return requestWithWinInet(url, method, body, headers);
#else
        response.error = "HTTP功能仅支持Windows平台";
        return response;
#endif
    }

private:
#ifdef _WIN32
    // 使用curl.exe处理请求（支持HTTPS）
    static HttpResponse requestWithCurl(const std::string& url,
                                       const std::string& method,
                                       const std::string& body,
                                       const std::map<std::string, std::string>& headers) {
        HttpResponse response;
        response.success = false;
        response.statusCode = 0;
        
        // 构建curl命令
        std::string cmd = "curl -s -i";
        
        // 添加方法
        if (method == "POST") {
            cmd += " -X POST";
            if (!body.empty()) {
                // 写入临时文件
                std::string tempFile = "_loong_post_body.tmp";
                FILE* f = fopen(tempFile.c_str(), "wb");
                if (f) {
                    fwrite(body.c_str(), 1, body.length(), f);
                    fclose(f);
                    cmd += " --data-binary \"@" + tempFile + "\"";
                }
            }
        }
        
        // 添加请求头
        for (const auto& h : headers) {
            cmd += " -H \"" + h.first + ": " + h.second + "\"";
        }
        
        // 添加URL
        cmd += " \"" + url + "\"";
        
        // 执行命令
        FILE* pipe = _popen(cmd.c_str(), "rb");
        if (!pipe) {
            response.error = "无法执行curl命令";
            return response;
        }
        
        // 读取响应
        std::array<char, 8192> buffer;
        std::string rawResponse;
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            rawResponse += buffer.data();
        }
        int exitCode = _pclose(pipe);
        
        // 清理临时文件
        if (method == "POST" && !body.empty()) {
            remove("_loong_post_body.tmp");
        }
        
        if (exitCode != 0) {
            response.error = "curl命令执行失败 (退出码: " + std::to_string(exitCode) + ")";
            return response;
        }
        
        // 解析响应
        parseCurlResponse(rawResponse, response);
        response.success = true;
        
        return response;
    }
    
    // 解析curl响应
    static void parseCurlResponse(const std::string& rawResponse, HttpResponse& response) {
        // 查找响应头和响应体的分隔
        size_t headerEnd = std::string::npos;
        
        // HTTP/1.1 响应可能有多个头部块（处理100 Continue等）
        size_t pos = 0;
        while ((pos = rawResponse.find("\r\n\r\n", pos)) != std::string::npos) {
            headerEnd = pos;
            pos += 4;
        }
        
        if (headerEnd == std::string::npos) {
            // 尝试查找 \n\n
            pos = 0;
            while ((pos = rawResponse.find("\n\n", pos)) != std::string::npos) {
                headerEnd = pos;
                pos += 2;
            }
        }
        
        if (headerEnd == std::string::npos) {
            response.body = rawResponse;
            return;
        }
        
        std::string headerSection = rawResponse.substr(0, headerEnd);
        response.body = rawResponse.substr(headerEnd + 4);
        
        // 解析状态行和头部
        std::istringstream headerStream(headerSection);
        std::string line;
        bool firstLine = true;
        
        while (std::getline(headerStream, line)) {
            // 移除\r
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            if (firstLine) {
                // 解析状态行 HTTP/1.1 200 OK
                firstLine = false;
                size_t spacePos = line.find(' ');
                if (spacePos != std::string::npos) {
                    size_t codeStart = line.find_first_not_of(' ', spacePos);
                    if (codeStart != std::string::npos) {
                        size_t codeEnd = line.find(' ', codeStart);
                        if (codeEnd == std::string::npos) {
                            codeEnd = line.length();
                        }
                        std::string codeStr = line.substr(codeStart, codeEnd - codeStart);
                        response.statusCode = std::stoi(codeStr);
                    }
                }
            } else {
                // 解析头部
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos) {
                    std::string key = line.substr(0, colonPos);
                    std::string value = line.substr(colonPos + 1);
                    // 去除空白
                    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
                        value = value.substr(1);
                    }
                    response.headers[key] = value;
                }
            }
        }
    }
    
    // 使用WinINet处理HTTP请求
    static HttpResponse requestWithWinInet(const std::string& url,
                                          const std::string& method,
                                          const std::string& body,
                                          const std::map<std::string, std::string>& headers) {
        HttpResponse response;
        response.success = false;
        response.statusCode = 0;
        
        // 初始化WinINet
        HINTERNET hInternet = InternetOpenA(
            "Loong/1.0",
            INTERNET_OPEN_TYPE_DIRECT,
            NULL, NULL, 0);
        
        if (!hInternet) {
            response.error = "无法初始化网络";
            return response;
        }
        
        // 设置超时
        DWORD timeout = 30000;
        InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        
        // 构建请求头字符串
        std::string headerStr;
        if (headers.find("User-Agent") == headers.end()) {
            headerStr += "User-Agent: Loong/1.0\r\n";
        }
        if (headers.find("Accept") == headers.end()) {
            headerStr += "Accept: */*\r\n";
        }
        if (headers.find("Connection") == headers.end()) {
            headerStr += "Connection: close\r\n";
        }
        for (const auto& h : headers) {
            headerStr += h.first + ": " + h.second + "\r\n";
        }
        
        // 设置请求标志
        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION;
        
        // 使用InternetOpenUrlA
        HINTERNET hUrl = InternetOpenUrlA(
            hInternet,
            url.c_str(),
            headerStr.empty() ? NULL : headerStr.c_str(),
            headerStr.empty() ? 0 : static_cast<DWORD>(headerStr.length()),
            flags,
            0);
        
        if (!hUrl) {
            char errorMsg[256] = {0};
            sprintf_s(errorMsg, sizeof(errorMsg), "HTTP请求失败 (错误: 0x%08lX)", GetLastError());
            InternetCloseHandle(hInternet);
            response.error = errorMsg;
            return response;
        }
        
        // 获取状态码
        DWORD statusCode = 0;
        DWORD size = sizeof(statusCode);
        HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                      &statusCode, &size, NULL);
        response.statusCode = static_cast<int>(statusCode);
        
        // 获取响应头
        char buffer[8192];
        DWORD bufferSize = sizeof(buffer);
        if (HttpQueryInfoA(hUrl, HTTP_QUERY_RAW_HEADERS_CRLF, buffer, &bufferSize, NULL)) {
            std::istringstream headerStream(buffer);
            std::string line;
            while (std::getline(headerStream, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);
                    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
                        value = value.substr(1);
                    }
                    response.headers[key] = value;
                }
            }
        }
        
        // 读取响应体
        char readBuffer[8192];
        DWORD bytesRead;
        while (InternetReadFile(hUrl, readBuffer, sizeof(readBuffer) - 1, &bytesRead)) {
            if (bytesRead == 0) break;
            readBuffer[bytesRead] = '\0';
            response.body += readBuffer;
        }
        
        // 清理
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        
        response.success = true;
        return response;
    }
#endif
};

} // namespace loong

#endif // LOONG_HTTP_HPP
