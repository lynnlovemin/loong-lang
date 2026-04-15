# Loong HTTP Server 标准库

基于 Loong 语言开发的 HTTP 服务器标准库，完整实现了 HTTP/1.1 协议的核心功能。

## 📦 模块结构

```
std/http/
├── utils.loong          # 工具函数模块
├── request.loong        # HTTP 请求解析模块
├── response.loong       # HTTP 响应构建模块
├── router.loong         # 路由模块
├── middleware.loong     # 中间件模块
├── static.loong         # 静态文件服务模块
├── server.loong         # HTTP 服务器核心模块
```

## 🎯 功能特性

### 1. 核心功能
- ✅ 完整的 HTTP/1.1 协议实现
- ✅ 支持所有 HTTP 方法：GET, POST, PUT, DELETE, OPTIONS
- ✅ URL 编码/解码
- ✅ 请求参数解析（query string）
- ✅ 路径参数支持（/:id）
- ✅ 请求体解析（form-data, JSON, URL-encoded）
- ✅ 文件上传支持（multipart/form-data）
- ✅ 二进制数据传输

### 2. 路由系统
- ✅ 支持动态路由参数
- ✅ 支持通配符路由
- ✅ 路由优先级匹配
- ✅ 404 自定义处理
- ✅ 路由中间件支持

### 3. 中间件
- ✅ 日志中间件（loggerMiddleware）
- ✅ CORS 中间件（corsMiddleware, simpleCorsMiddleware）
- ✅ 错误处理中间件（errorHandlerMiddleware）
- ✅ JSON 解析中间件（jsonMiddleware）
- ✅ URL-encoded 解析中间件（urlencodedMiddleware）
- ✅ 基础认证中间件（basicAuthMiddleware）
- ✅ Token 认证中间件（tokenAuthMiddleware）
- ✅ 限流中间件（rateLimitMiddleware）
- ✅ 会话中间件（sessionMiddleware）
- ✅ 压缩中间件（compressMiddleware）
- ✅ 安全头部中间件（securityHeadersMiddleware）
- ✅ 请求 ID 中间件（requestIdMiddleware）

### 4. 静态文件服务
- ✅ 自动 MIME 类型识别（16 种常用类型）
- ✅ 默认索引文件（index.html）
- ✅ 目录浏览支持
- ✅ 路径安全检查（防止目录遍历攻击）
- ✅ 文件缓存控制

### 5. HTTP 服务器
- ✅ TCP Socket 服务器
- ✅ 多连接处理
- ✅ 请求超时控制
- ✅ Keep-Alive 支持
- ✅ 并发连接限制
- ✅ 客户端信息获取
- ✅ 访问日志记录

## 📖 使用示例

### 快速开始

```loong
import http.server

// 创建服务器
val app = server.createServer("0.0.0.0", 8080)

// 定义处理函数
fn homeHandler(req, res) {
    res.text("Hello World!")
}

fn healthHandler(req, res) {
    res.json("{\"status\":\"healthy\"}")
}

// 注册路由
app.get("/", homeHandler)
app.get("/api/health", healthHandler)

// 启动服务器
app.start()
```

### 完整示例

```loong
import  http.server
import  http.middleware
import  http.static

// 创建服务器
val app = server.createServer("0.0.0.0", 8080)

// 配置中间件
app.use(middleware.loggerMiddleware)
app.use(middleware.simpleCorsMiddleware)
app.use(middleware.securityHeadersMiddleware)

// 定义处理函数
fn homeHandler(req, res) {
    res.text("Welcome!")
}

fn usersHandler(req, res) {
    res.json("{\"users\":[]}")
}

fn userHandler(req, res) {
    val userId = req.getPathParam("id")
    res.text("User ID: " + userId)
}

fn dataHandler(req, res) {
    val body = req.body
    res.json("{\"status\":\"success\"}")
}

fn notFoundHandler(req, res) {
    res.statusCode = 404
    res.text("Not Found")
}

// 注册路由
app.get("/", homeHandler)
app.get("/api/users", usersHandler)
app.get("/user/:id", userHandler)
app.post("/api/data", dataHandler)

// 404 处理
app.notFound(notFoundHandler)

// 启动服务器
app.start()
```

### 静态文件服务

```loong
import  http.server
import  http.static

val app = server.createServer("0.0.0.0", 8080)

// 创建静态文件服务器
val staticServer = static.createStaticFileServer("./public", nil)

// 使用静态文件中间件
// app.use(staticServer.handle)

app.start()
```

### 中间件使用

```loong
import  http.server
import  http.middleware

val app = server.createServer("0.0.0.0", 8080)

// 日志中间件
app.use(middleware.loggerMiddleware)

// CORS 中间件
app.use(middleware.simpleCorsMiddleware)

// 安全头部中间件
app.use(middleware.securityHeadersMiddleware)

// 自定义中间件
fn customMiddleware(req, res) {
    println("Custom middleware: ", req.path)
    return true  // 继续处理
}

app.use(customMiddleware)

app.start()
```

## 🔧 API 文档

### HttpServer 类

#### 构造函数
```loong
val server = server.createServer(host, port)
```

#### 路由方法
- `get(path, handler)` - GET 路由
- `post(path, handler)` - POST 路由
- `put(path, handler)` - PUT 路由
- `delete(path, handler)` - DELETE 路由
- `options(path, handler)` - OPTIONS 路由
- `all(path, handler)` - 所有方法路由

#### 中间件方法
- `use(middleware)` - 注册中间件

#### 配置方法
- `setConfig(key, value)` - 设置配置项
- `getConfig(key)` - 获取配置项

#### 其他方法
- `notFound(handler)` - 设置 404 处理函数
- `start()` - 启动服务器
- `stop()` - 停止服务器
- `getInfo()` - 获取服务器信息
- `printRoutes()` - 打印路由列表

### HttpRequest 类

#### 属性
- `method` - HTTP 方法
- `path` - 请求路径
- `query` - 查询参数字典
- `headers` - 请求头
- `body` - 请求体
- `clientIp` - 客户端 IP
- `clientPort` - 客户端端口

#### 方法
- `getHeader(name)` - 获取请求头
- `getQueryParam(name)` - 获取查询参数
- `getPathParam(name)` - 获取路径参数
- `setPathParams(keys, values)` - 设置路径参数

### HttpResponse 类

#### 属性
- `statusCode` - 状态码
- `headers` - 响应头
- `body` - 响应体

#### 方法
- `setHeader(name, value)` - 设置响应头
- `getHeader(name)` - 获取响应头
- `contentType(type)` - 设置 Content-Type
- `text(content)` - 发送文本响应
- `json(data)` - 发送 JSON 响应
- `html(content)` - 发送 HTML 响应
- `file(path)` - 发送文件响应
- `sendError(code, message)` - 发送错误响应
- `build()` - 构建完整响应

## 🧪 测试

### 运行集成测试
```bash
loong test_all_features.loong
```

## 📝 配置选项

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| maxConnections | 100 | 最大并发连接数 |
| timeout | 30000 | 请求超时时间（毫秒） |
| keepAlive | false | 是否启用 Keep-Alive |

## 🎨 MIME 类型支持

| 扩展名 | MIME 类型 |
|--------|-----------|
| html | text/html; charset=utf-8 |
| css | text/css; charset=utf-8 |
| js | application/javascript; charset=utf-8 |
| json | application/json; charset=utf-8 |
| png | image/png |
| jpg/jpeg | image/jpeg |
| gif | image/gif |
| ico | image/x-icon |
| svg | image/svg+xml |
| txt | text/plain; charset=utf-8 |
| xml | application/xml |
| pdf | application/pdf |
| zip | application/zip |
| tar | application/x-tar |
| gz | application/gzip |

## ⚠️ 注意事项

1. **匿名函数限制**：Loong 语言不支持内联匿名函数，需要先定义函数再注册
2. **TCP API**：使用 Loong 的 tcp_server_create() 等 API 创建 TCP 服务器
3. **文件操作**：静态文件服务需要实际的文件目录
4. **并发处理**：当前版本为单线程，适合开发和测试环境

## 🚀 开发计划

- [ ] 支持 HTTPS
- [ ] WebSocket 支持
- [ ] HTTP/2 支持
- [ ] 更完善的中间件生态
- [ ] 性能优化
- [ ] 更多测试用例

## 📄 License

MIT License
