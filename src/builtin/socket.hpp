#ifndef LOONG_SOCKET_HPP
#define LOONG_SOCKET_HPP

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define CLOSE_SOCKET(s) close(s)
#endif

#include <string>
#include <map>
#include <vector>

namespace loong {

// Socket类型枚举
enum class SocketType {
    TCP,
    UDP
};

// Socket类
class Socket {
private:
    socket_t sock;
    SocketType type;
    bool connected;
    std::string host;
    int port;
    int timeout; // 毫秒
    
public:
    Socket() : sock(INVALID_SOCKET_VALUE), type(SocketType::TCP), connected(false), port(0), timeout(30000) {
        initWinsock();
    }
    
    Socket(SocketType t) : sock(INVALID_SOCKET_VALUE), type(t), connected(false), port(0), timeout(30000) {
        initWinsock();
    }
    
    ~Socket() {
        close();
    }
    
    // 初始化Winsock（Windows）
    static void initWinsock() {
#ifdef _WIN32
        static bool initialized = false;
        if (!initialized) {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
            initialized = true;
        }
#endif
    }
    
    // 设置超时
    void setTimeout(int ms) {
        timeout = ms;
    }
    
    // 创建Socket
    bool create() {
        int sockType = (type == SocketType::TCP) ? SOCK_STREAM : SOCK_DGRAM;
        int protocol = (type == SocketType::TCP) ? IPPROTO_TCP : IPPROTO_UDP;
        
        sock = socket(AF_INET, sockType, protocol);
        if (sock == INVALID_SOCKET_VALUE) {
            return false;
        }
        
        // 设置超时
#ifdef _WIN32
        DWORD tv = timeout;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
        struct timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
        
        return true;
    }
    
    // 连接到服务器
    bool connect(const std::string& host, int port) {
        if (sock == INVALID_SOCKET_VALUE) {
            if (!create()) return false;
        }
        
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        
        // 解析主机名
        struct hostent* he = gethostbyname(host.c_str());
        if (he == nullptr) {
            // 尝试直接转换IP地址
            serverAddr.sin_addr.s_addr = inet_addr(host.c_str());
            if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
                return false;
            }
        } else {
            memcpy(&serverAddr.sin_addr, he->h_addr_list[0], he->h_length);
        }
        
        if (::connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR_VALUE) {
            return false;
        }
        
        this->host = host;
        this->port = port;
        this->connected = true;
        return true;
    }
    
    // 绑定端口
    bool bind(int port) {
        if (sock == INVALID_SOCKET_VALUE) {
            if (!create()) return false;
        }
        
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);
        
        if (::bind(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR_VALUE) {
            return false;
        }
        
        this->port = port;
        return true;
    }
    
    // 监听（TCP服务器）
    bool listen(int backlog = 5) {
        if (sock == INVALID_SOCKET_VALUE) return false;
        return ::listen(sock, backlog) != SOCKET_ERROR_VALUE;
    }
    
    // 接受连接（TCP服务器）
    Socket* accept() {
        struct sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        
        socket_t clientSock = ::accept(sock, (struct sockaddr*)&clientAddr, &addrLen);
        if (clientSock == INVALID_SOCKET_VALUE) {
            return nullptr;
        }
        
        Socket* client = new Socket(type);
        client->sock = clientSock;
        client->connected = true;
        client->host = inet_ntoa(clientAddr.sin_addr);
        client->port = ntohs(clientAddr.sin_port);
        
        return client;
    }
    
    // 发送数据
    int send(const std::string& data) {
        if (sock == INVALID_SOCKET_VALUE) return -1;
        return ::send(sock, data.c_str(), (int)data.length(), 0);
    }
    
    // 发送数据到指定地址（UDP）
    int sendTo(const std::string& data, const std::string& host, int port) {
        if (sock == INVALID_SOCKET_VALUE) {
            if (!create()) return -1;
        }
        
        struct sockaddr_in destAddr;
        memset(&destAddr, 0, sizeof(destAddr));
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(port);
        destAddr.sin_addr.s_addr = inet_addr(host.c_str());
        
        if (destAddr.sin_addr.s_addr == INADDR_NONE) {
            struct hostent* he = gethostbyname(host.c_str());
            if (he == nullptr) return -1;
            memcpy(&destAddr.sin_addr, he->h_addr_list[0], he->h_length);
        }
        
        return sendto(sock, data.c_str(), (int)data.length(), 0, 
                      (struct sockaddr*)&destAddr, sizeof(destAddr));
    }
    
    // 接收数据
    std::string recv(int bufferSize = 4096) {
        if (sock == INVALID_SOCKET_VALUE) return "";
        
        std::vector<char> buffer(bufferSize + 1);
        int received = ::recv(sock, buffer.data(), bufferSize, 0);
        
        if (received <= 0) {
            return "";
        }
        
        buffer[received] = '\0';
        return std::string(buffer.data());
    }
    
    // 接收所有数据
    std::string recvAll(int maxBytes = 1024 * 1024) {
        if (sock == INVALID_SOCKET_VALUE) return "";
        
        std::string result;
        char buffer[4096];
        int totalReceived = 0;
        
        while (totalReceived < maxBytes) {
            int received = ::recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            result += buffer;
            totalReceived += received;
        }
        
        return result;
    }
    
    // 接收来自的数据（UDP）
    std::string recvFrom(std::string& senderHost, int& senderPort, int bufferSize = 4096) {
        if (sock == INVALID_SOCKET_VALUE) return "";
        
        struct sockaddr_in senderAddr;
        int addrLen = sizeof(senderAddr);
        
        std::vector<char> buffer(bufferSize + 1);
        int received = recvfrom(sock, buffer.data(), bufferSize, 0,
                               (struct sockaddr*)&senderAddr, &addrLen);
        
        if (received <= 0) {
            return "";
        }
        
        buffer[received] = '\0';
        senderHost = inet_ntoa(senderAddr.sin_addr);
        senderPort = ntohs(senderAddr.sin_port);
        
        return std::string(buffer.data());
    }
    
    // 关闭连接
    void close() {
        if (sock != INVALID_SOCKET_VALUE) {
            CLOSE_SOCKET(sock);
            sock = INVALID_SOCKET_VALUE;
        }
        connected = false;
    }
    
    // 检查是否已连接
    bool isConnected() const {
        return connected && sock != INVALID_SOCKET_VALUE;
    }
    
    // 获取Socket描述符
    socket_t getHandle() const {
        return sock;
    }
    
    // 获取主机
    std::string getHost() const {
        return host;
    }
    
    // 获取端口
    int getPort() const {
        return port;
    }
    
    // 获取类型
    SocketType getType() const {
        return type;
    }
};

// TCP客户端类
class TcpClient {
private:
    Socket socket;
    
public:
    TcpClient() : socket(SocketType::TCP) {}
    
    bool connect(const std::string& host, int port) {
        return socket.connect(host, port);
    }
    
    int send(const std::string& data) {
        return socket.send(data);
    }
    
    std::string recv(int bufferSize = 4096) {
        return socket.recv(bufferSize);
    }
    
    std::string recvAll(int maxBytes = 1024 * 1024) {
        return socket.recvAll(maxBytes);
    }
    
    void close() {
        socket.close();
    }
    
    bool isConnected() const {
        return socket.isConnected();
    }
    
    void setTimeout(int ms) {
        socket.setTimeout(ms);
    }
};

// TCP服务器类
class TcpServer {
private:
    Socket socket;
    bool listening;
    
public:
    TcpServer() : socket(SocketType::TCP), listening(false) {}
    
    bool bind(int port) {
        return socket.bind(port);
    }
    
    bool listen(int backlog = 5) {
        if (socket.listen(backlog)) {
            listening = true;
            return true;
        }
        return false;
    }
    
    Socket* accept() {
        return socket.accept();
    }
    
    void close() {
        socket.close();
        listening = false;
    }
    
    bool isListening() const {
        return listening;
    }
};

// UDP客户端类
class UdpClient {
private:
    Socket socket;
    
public:
    UdpClient() : socket(SocketType::UDP) {
        socket.create();
    }
    
    int send(const std::string& data, const std::string& host, int port) {
        return socket.sendTo(data, host, port);
    }
    
    std::string recv(std::string& senderHost, int& senderPort, int bufferSize = 4096) {
        return socket.recvFrom(senderHost, senderPort, bufferSize);
    }
    
    bool bind(int port) {
        return socket.bind(port);
    }
    
    void close() {
        socket.close();
    }
    
    void setTimeout(int ms) {
        socket.setTimeout(ms);
    }
};

// DNS解析
inline std::string dnsResolve(const std::string& hostname) {
    Socket::initWinsock();
    
    struct hostent* he = gethostbyname(hostname.c_str());
    if (he == nullptr) {
        return "";
    }
    
    struct in_addr addr;
    memcpy(&addr, he->h_addr_list[0], sizeof(struct in_addr));
    return std::string(inet_ntoa(addr));
}

// 反向DNS解析
inline std::string dnsReverse(const std::string& ip) {
    Socket::initWinsock();
    
    struct in_addr addr;
    addr.s_addr = inet_addr(ip.c_str());
    if (addr.s_addr == INADDR_NONE) {
        return "";
    }
    
    struct hostent* he = gethostbyaddr((const char*)&addr, sizeof(addr), AF_INET);
    if (he == nullptr) {
        return "";
    }
    
    return std::string(he->h_name);
}

// 检查端口是否开放
inline bool isPortOpen(const std::string& host, int port, int timeoutMs = 3000) {
    Socket sock(SocketType::TCP);
    sock.setTimeout(timeoutMs);
    
    if (!sock.create()) {
        return false;
    }
    
    bool result = sock.connect(host, port);
    sock.close();
    
    return result;
}

} // namespace loong

#endif // LOONG_SOCKET_HPP
