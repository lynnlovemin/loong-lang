#ifndef LOONG_HTTP_HPP
#define LOONG_HTTP_HPP

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <wininet.h>
    #pragma comment(lib, "wininet.lib")
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
        // 解析URL
        std::string server, path;
        int port = 80;
        bool isHttps = false;
        
        if (!parseUrl(url, server, path, port, isHttps)) {
            response.error = "无效的URL";
            return response;
        }
        
        // 初始化WinINet
        HINTERNET hInternet = InternetOpenA(
            "Loong/1.0",
            INTERNET_OPEN_TYPE_DIRECT,
            NULL, NULL, 0);
        
        if (!hInternet) {
            response.error = "无法初始化网络";
            return response;
        }
        
        // 连接服务器
        HINTERNET hConnect = InternetConnectA(
            hInternet,
            server.c_str(),
            port,
            NULL, NULL,
            INTERNET_SERVICE_HTTP,
            0, 0);
        
        if (!hConnect) {
            InternetCloseHandle(hInternet);
            response.error = "无法连接服务器";
            return response;
        }
        
        // 设置请求标志
        DWORD flags = 0;
        if (isHttps) {
            flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
        }
        
        // 创建请求
        HINTERNET hRequest = HttpOpenRequestA(
            hConnect,
            method.c_str(),
            path.c_str(),
            NULL, NULL, NULL,
            flags, 0);
        
        if (!hRequest) {
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            response.error = "无法创建请求";
            return response;
        }
        
        // 添加请求头
        std::string headerStr;
        for (const auto& h : headers) {
            headerStr += h.first + ": " + h.second + "\r\n";
        }
        if (!headerStr.empty()) {
            HttpAddRequestHeadersA(hRequest, headerStr.c_str(), -1, HTTP_ADDREQ_FLAG_ADD);
        }
        
        // 发送请求
        BOOL result = HttpSendRequestA(
            hRequest,
            NULL, 0,
            (LPVOID)(body.empty() ? NULL : body.c_str()),
            body.length());
        
        if (!result) {
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            response.error = "请求发送失败";
            return response;
        }
        
        // 获取状态码
        DWORD statusCode = 0;
        DWORD size = sizeof(statusCode);
        HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                      &statusCode, &size, NULL);
        response.statusCode = statusCode;
        
        // 获取响应头
        char buffer[4096];
        DWORD bufferSize = sizeof(buffer);
        if (HttpQueryInfoA(hRequest, HTTP_QUERY_RAW_HEADERS, buffer, &bufferSize, NULL)) {
            std::istringstream headerStream(buffer);
            std::string line;
            while (std::getline(headerStream, line)) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);
                    // 去除首尾空白
                    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
                    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value = value.substr(1);
                    response.headers[key] = value;
                }
            }
        }
        
        // 读取响应体
        char readBuffer[8192];
        DWORD bytesRead;
        while (InternetReadFile(hRequest, readBuffer, sizeof(readBuffer) - 1, &bytesRead)) {
            if (bytesRead == 0) break;
            readBuffer[bytesRead] = '\0';
            response.body += readBuffer;
        }
        
        // 清理
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        
        response.success = true;
#else
        response.error = "HTTP功能仅支持Windows平台";
#endif
        return response;
    }

private:
    static bool parseUrl(const std::string& url, std::string& server, 
                        std::string& path, int& port, bool& isHttps) {
        isHttps = false;
        port = 80;
        path = "/";
        
        size_t pos = 0;
        
        // 检查协议
        if (url.substr(0, 7) == "http://") {
            pos = 7;
        } else if (url.substr(0, 8) == "https://") {
            pos = 8;
            isHttps = true;
            port = 443;
        } else {
            return false;
        }
        
        // 找到服务器名结束位置
        size_t serverEnd = url.find('/', pos);
        if (serverEnd == std::string::npos) {
            server = url.substr(pos);
            path = "/";
        } else {
            server = url.substr(pos, serverEnd - pos);
            path = url.substr(serverEnd);
        }
        
        // 检查端口
        size_t portPos = server.find(':');
        if (portPos != std::string::npos) {
            port = std::stoi(server.substr(portPos + 1));
            server = server.substr(0, portPos);
        }
        
        return true;
    }
};

} // namespace loong

#endif // LOONG_HTTP_HPP
