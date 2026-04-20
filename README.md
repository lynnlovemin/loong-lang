# Loong 编程语言 v1.3.3

## 简介

**Loong**（龙）是一款解释型编程语言，寓意为龙。采用C++17开发，具有简洁优雅的语法。

## 下载安装

📥 **[下载 Windows 安装包](loong-1.3.3-setup.exe)** (v1.3.3)

安装包功能：
- 自动安装 loong.exe 到指定目录
- 包含 std 标准库
- 可选添加到系统 PATH 环境变量

## 语法特点

| 特性 | Loong | Python |
|------|-------|--------|
| 变量声明 | `val x = 10;` | `x = 10` |
| 代码块 | `{ }` | 缩进 |
| 函数定义 | `fn name() {}` | `def name():` |
| 分支 | `if cond {} else {}` | `if cond:` |
| For循环 | `for init; cond; update {}` | `for x in range():` |
| 注释 | `//` 和 `/* */` | `#` |
| 语句结束 | 分号可选 | 换行 |

### 运算符

**逻辑运算符**
| 运算符 | 说明 | 示例 |
|--------|------|------|
| `&&` / `and` | 逻辑与 | `a && b` |
| `\|\|` / `or` | 逻辑或 | `a \|\| b` |
| `!` / `not` | 逻辑非 | `!a` |

**位运算符**
| 运算符 | 说明 | 示例 |
|--------|------|------|
| `&` | 位与 | `5 & 3` → `1` |
| `\|` | 位或 | `5 \| 3` → `7` |
| `^` | 异或 | `5 ^ 3` → `6` |
| `~` | 位取反 | `~0` → `-1` |
| `<<` | 左移 | `1 << 4` → `16` |
| `>>` | 右移 | `16 >> 2` → `4` |

**三元运算符**
| 运算符 | 说明 | 示例 |
|--------|------|------|
| `? :` | 条件表达式 | `cond ? trueVal : falseVal` |

## 编译

### 依赖
- CMake 3.10+
- C++17 编译器 (MSVC/GCC/Clang)

### Windows
```batch
build.bat
```

### 手动编译
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## 使用

### 执行脚本文件
```bash
loong hello.loong
```

### 交互模式 (REPL)
```bash
loong
```

### 执行代码字符串
```bash
loong -c "println('Hello, Loong!')"
```

### 查看帮助
```bash
loong --help
```

## 示例代码

### Hello World
```loong
println("Hello, Loong!");
```

### 变量声明
```loong
val name = "Loong";
val version = 1.0;
val flag = true;
val list = [1, 2, 3];
val dict = {"key": "value"};
val ch = 'A';  // 字符类型
```

### 常量声明
新增 const 常量语法支持，用于定义不可修改的常量值。

```loong
// 基本用法
const PI = 3.14159;
const MAX_SIZE = 100;
const APP_NAME = "MyApp";

// 常量无法被修改，以下代码会报错：
// PI = 3.14;  // 错误：无法修改常量

// 常量支持所有类型
const FLAG = true;
const LIST = [1, 2, 3];
const DICT = {"key": "value"};
const CH = 'A';

// 常量在作用域内有效
fn example() {
    const LOCAL_CONST = 42;
    println(LOCAL_CONST);
}
```

**特性**
- 常量必须在声明时初始化
- 常量一旦赋值就不能被修改
- 常量支持所有数据类型
- 常量遵循作用域规则

### 字符类型和ASCII码
```loong
// 字符字面量
val c1 = 'A';
val c2 = '\n';  // 转义字符
val c3 = '\t';  // 制表符

// 字符与整数运算（自动转换为ASCII码）
val result = 'A' + 1;    // 结果为66
val diff = 'Z' - 'A';    // 结果为25

// int函数：字符转ASCII码
val code = int('A');     // 结果为65

// chr函数：整数转字符
val letter = chr(65);    // 结果为 'A'

// 比较字符
if 'a' < 'z' {
    println("'a' 小于 'z'");
}
```

### 函数定义
```loong
fn add(a, b) {
    return a + b;
}

fn greet(name = "World") {
    return "Hello, " + name;
}
```

### 匿名函数和 Lambda 表达式

Loong v1.3.2 新增匿名函数和 Lambda 表达式支持

**匿名函数** - 使用 `fn` 关键字定义
```loong
// 基本语法
val add = fn(a, b) {
    return a + b;
};

// 作为回调参数
handler(fn(req, res) {
    res.write("Hello");
});

// 支持默认参数
val greet = fn(name, greeting = "Hello") {
    return greeting + ", " + name + "!";
};
```

**Lambda 表达式** - 使用 `->` 箭头语法
```loong
// 单表达式 Lambda（自动返回）
val square = x -> x * x;
val add = (a, b) -> a + b;

// 无参数 Lambda
val getPi = () -> 3.14159;

// 块体 Lambda（需要显式 return）
val factorial = n -> {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
};

// Lambda 作为回调
val doubled = map([1, 2, 3], x -> x * 2);
val evens = filter([1, 2, 3, 4, 5], x -> x % 2 == 0);

// 立即执行 Lambda
val result = (() -> {
    val temp = 42;
    return temp * 2;
})();
```

**闭包捕获**
```loong
// 自动捕获外部变量
val factor = 10;
val scale = x -> x * factor;  // 捕获 factor

// 工厂函数
fn makeMultiplier(n) {
    return x -> x * n;  // 捕获参数 n
}
val double = makeMultiplier(2);
println(double(5));  // 输出: 10
```

**高阶函数示例**
```loong
// map 函数
fn map(list, mapper) {
    val result = [];
    for val item in list {
        result.push(mapper(item));
    }
    return result;
}

// filter 函数
fn filter(list, predicate) {
    val result = [];
    for val item in list {
        if predicate(item) {
            result.push(item);
        }
    }
    return result;
}

// reduce 函数
fn reduce(list, initial, reducer) {
    val acc = initial;
    for val item in list {
        acc = reducer(acc, item);
    }
    return acc;
}

// 使用示例
val nums = [1, 2, 3, 4, 5];
val doubled = map(nums, x -> x * 2);         // [2, 4, 6, 8, 10]
val evens = filter(nums, x -> x % 2 == 0);   // [2, 4]
val sum = reduce(nums, 0, (a, b) -> a + b);  // 15
```

**语法对比**

| 特性 | 匿名函数 | Lambda |
|------|---------|--------|
| 语法 | `fn(params) { body }` | `(params) -> expr` 或 `(params) -> { body }` |
| 单参数简写 | 不支持 | `x -> expr` |
| 无参数 | `fn() { }` | `() -> expr` |
| 返回值 | 需显式 `return` | 表达式体自动返回，块体需显式 `return` |
| 默认参数 | 支持 | 支持 |
| 闭包 | 支持 | 支持 |

### 条件语句
```loong
if x > 10 {
    println("x is greater than 10");
} elif x > 5 {
    println("x is greater than 5");
} else {
    println("x is 5 or less");
}
```

### Switch/Case 语句
Loong v1.3.1 新增 switch/case 语法支持，提供多分支选择结构。

**基本语法**
```loong
switch day {
    case "Monday":
        println("Start of work week");
    case "Friday":
        println("Almost weekend");
    case "Saturday":
    case "Sunday":
        println("Weekend!");
    default:
        println("Regular day");
}
```

**支持的类型**
- 数字（整数和浮点数）
- 字符串
- 字符
- 布尔值
- 大整数

**特性**
- 自动 break：每个 case 执行完后自动退出 switch（无需手动 break）
- 支持 default 分支：当没有匹配的 case 时执行
- 支持多 case 共享同一代码块
- 类型安全：switch 值和 case 值必须类型相同才能匹配

**数字示例**
```loong
val score = 85;

switch score {
    case 100:
        println("Perfect!");
    case 90:
        println("Excellent");
    case 80:
        println("Good");
    default:
        println("Keep trying");
}
```

**字符串示例**
```loong
val fruit = "apple";

switch fruit {
    case "apple":
        println("It's an apple");
    case "banana":
        println("It's a banana");
    case "orange":
        println("It's an orange");
    default:
        println("Unknown fruit");
}
```

### 三元运算符
新增三元运算符语法支持，提供简洁的条件表达式。

**基本语法**
```loong
val result = condition ? trueValue : falseValue;
```

**示例**
```loong
// 基本用法
val age = 20;
val status = age >= 18 ? "成年" : "未成年";
println(status);  // 输出: 成年

// 嵌套使用
val score = 85;
val grade = score >= 90 ? "优秀" : (score >= 60 ? "及格" : "不及格");
println(grade);  // 输出: 良好

// 在表达式中使用
val x = 10;
val y = 20;
val max = x > y ? x : y;
println("最大值: " + max);  // 输出: 最大值: 20

// 与函数调用结合
val isLoggedIn = true;
println(isLoggedIn ? "欢迎回来" : "请登录");
```

**特性**
- 短路求值：只计算被选择的分支
- 支持任意表达式作为条件和结果
- 支持嵌套使用
- 优先级低于逻辑运算符，高于赋值运算符

### 循环
```loong
// 传统 for 循环
for val i = 0; i < 10; i = i + 1 {
    println(i);
}

// for-in 循环
for val item in [1, 2, 3] {
    println(item);
}

// while 循环
while x > 0 {
    println(x);
    x = x - 1;
}
```

### 模块导入

```loong
// 导入标准库模块
import math

println(math.PI);           // 3.14159...
println(math.pow(2, 3));    // 8

// 使用点分模块名导入多层目录模块
// 对应文件: loong.exe目录/std/dragonfire/drivers/driver.loong
import dragonfire.drivers.driver

driver.connect();           // 使用 driver. 访问模块成员

// 使用 import from 指定路径导入
import mylib from "lib/mylib.loong"
println(mylib.greet("Loong"));

// 使用 ./ 相对路径导入（仅从当前源文件目录寻址）
import helper from "./helper.loong"

// 使用 ../ 向上一级目录寻址（仅从当前源文件目录寻址）
import utils from "../utils.loong"

// 导入网络爬虫库
import crawler from "std/crawler.loong"
val resp = crawler.fetch("http://example.com");
```

#### 模块寻址规则

**`import 模块名` 自动寻址（支持点分模块名）：**

1. **源文件当前目录** - 优先从当前执行的 `.loong` 文件所在目录查找
2. **loong.exe 安装目录的 std** - 如果当前目录未找到，从解释器安装目录下的 `std/` 查找

点分模块名会自动转换为目录路径，例如：
- `import math` → 查找 `math.loong`
- `import dragonfire.drivers.driver` → 查找 `dragonfire/drivers/driver.loong`

**`import 模块名 from "路径"` 指定路径：**

- 绝对路径直接使用
- 以 `./` 或 `../` 开头的相对路径：仅基于当前源文件目录解析，**不搜索 std 标准库**
- 其他相对路径：先从当前源文件目录解析，未找到则从 `loong.exe/std/` 解析

| 路径写法 | 说明 | 是否搜索 std |
|---------|------|-------------|
| `"./helper.loong"` | 当前目录 | 否 |
| `"../utils.loong"` | 上级目录 | 否 |
| `"../../lib/util.loong"` | 上两级目录 | 否 |
| `"lib/mylib.loong"` | 相对路径 | 是 |
| `"std/crawler.loong"` | 标准库路径 | 是 |

### 异常处理
```loong
// try-catch
try {
    throw "发生错误";
} catch (err) {
    println("捕获异常: ", err);
}

// try-catch-finally
try {
    riskyOperation();
} catch (e) {
    handleError(e);
} finally {
    cleanup();
}

// 只有finally
try {
    doSomething();
} finally {
    cleanup();
}
```

### 类和对象
```loong
// 类定义
class Point {
    fn __init__(x, y) {
        self.x = x;
        self.y = y;
    }
    
    fn toString() {
        return "Point(" + str(self.x) + ", " + str(self.y) + ")";
    }
    
    fn distance() {
        return sqrt(self.x * self.x + self.y * self.y);
    }
}

// 创建对象
val p = Point(3, 4);
println(p.toString());  // Point(3, 4)
println(p.distance());  // 5

// 继承
class Point3D : Point {
    fn __init__(x, y, z) {
        super.__init__(x, y);
        self.z = z;
    }
    
    fn toString() {
        return "Point3D(" + str(self.x) + ", " + str(self.y) + ", " + str(self.z) + ")";
    }
}

val p3 = Point3D(1, 2, 3);
println(p3.toString());  // Point3D(1, 2, 3)
```

### HTTP网络请求
```loong
// 使用爬虫标准库
import crawler from "std/crawler.loong";

// 快速GET请求
val resp = fetch("http://httpbin.org/get");
println("状态码: ", resp.status);
println("响应: ", resp.body);

// 使用Crawler类
val crawler = Crawler();
crawler.setHeader("User-Agent", "Loong/1.0");
crawler.setHeader("Accept", "application/json");
val resp2 = crawler.get("http://httpbin.org/headers");

// POST请求
val resp3 = post("http://httpbin.org/post", "name=Loong");

// 解析HTML
val html = "<html><body><a href='link1.html'>链接</a></body></html>";
val links = html_links(html);  // 提取所有链接
val text = html_text(html);    // 提取纯文本
```

## 内置函数

### 输入输出
- `print(...)` - 打印（不换行）
- `println(...)` - 打印（换行）
- `input(prompt)` - 读取输入

### 类型转换
- `type(x)` - 获取类型名称
- `str(x)` - 转字符串
- `num(x)` - 转数字
- `bool(x)` - 转布尔值
- `int(x)` - 转整数（支持字符转ASCII码）
- `chr(x)` - 将整数转换为ASCII字符

### 类型判断
- `is_null(x)` - 判断是否为nil
- `is_number(x)` - 判断是否为数字
- `is_string(x)` - 判断是否为字符串
- `is_list(x)` - 判断是否为列表
- `is_dict(x)` - 判断是否为字典
- `is_function(x)` - 判断是否为函数

### 数学函数
- `abs(x)` - 绝对值
- `sqrt(x)` - 平方根
- `pow(base, exp)` - 幂运算
- `log(x)` - 自然对数
- `log10(x)` - 以10为底对数
- `sin(x)` - 正弦
- `cos(x)` - 余弦
- `tan(x)` - 正切
- `asin(x)` - 反正弦
- `acos(x)` - 反余弦
- `atan(x)` - 反正切
- `floor(x)` - 向下取整
- `ceil(x)` - 向上取整
- `round(x)` - 四舍五入
- `random()` - 随机数(0-1)
- `random_int(min, max)` - 随机整数
- `PI` - 圆周率常量
- `E` - 自然常数

### 字符串函数
- `len(x)` - 获取长度
- `split(str, sep)` - 分割字符串
- `join(list, sep)` - 连接列表为字符串
- `trim(str)` - 去除首尾空白
- `replace(str, old, new)` - 替换子串
- `upper(str)` - 转大写
- `lower(str)` - 转小写
- `startswith(str, prefix)` - 检查前缀
- `endswith(str, suffix)` - 检查后缀
- `find(str, substr)` - 查找子串位置
- `substr(str, start, end)` - 截取子串

### 列表函数
- `range(n)` - 生成范围列表
- `push(list, item)` - 添加元素
- `pop(list)` - 弹出末尾元素
- `first(list)` - 获取第一个元素
- `last(list)` - 获取最后一个元素
- `reverse(list)` - 反转列表
- `slice(list, start, end)` - 切片
- `concat(list1, list2)` - 连接列表
- `insert(list, index, item)` - 插入元素
- `remove_at(list, index)` - 删除指定索引元素

### 列表/字典/字符串方法
列表方法：
- `list.push(item)` - 添加元素到末尾
- `list.pop()` - 弹出末尾元素
- `list.insert(index, item)` - 在指定位置插入元素
- `list.remove(index)` - 删除指定位置元素
- `list.join(sep?)` - 连接为字符串

字典方法：
- `dict.keys()` - 获取所有键的列表
- `dict.values()` - 获取所有值的列表
- `dict.has(key)` - 检查键是否存在
- `dict.remove(key)` - 删除指定键

字符串方法：
- `str.contains(substr)` - 检查是否包含子串
- `str.startsWith(prefix)` - 检查前缀
- `str.endsWith(suffix)` - 检查后缀
- `str.replace(old, new)` - 替换子串
- `str.split(sep?)` - 分割字符串
- `str.trim()` - 去除首尾空白

### 文件IO
- `file_read(path)` - 读取文件内容
- `file_write(path, content)` - 写入文件
- `file_append(path, content)` - 追加内容
- `file_exists(path)` - 检查文件是否存在
- `file_delete(path)` - 删除文件

### 时间函数
- `time_now()` - 当前时间戳(秒)
- `time_ms()` - 当前时间戳(毫秒)
- `sleep(ms)` - 休眠毫秒

### HTTP网络请求
- `http_get(url, headers?)` - GET请求，返回响应字典
- `http_post(url, body, headers?)` - POST请求
- `http_request(url, method, body, headers)` - 通用HTTP请求

### HTML解析
- `html_tag(html, tagName)` - 提取所有指定标签内容
- `html_attr(tag, attrName)` - 提取标签属性值
- `html_text(html)` - 提取纯文本（去除标签）
- `html_links(html)` - 提取所有链接

### URL工具
- `url_encode(str)` - URL编码
- `url_decode(str)` - URL解码

### 网络协议（TCP/UDP/Socket）
Loong v1.1.0 提供底层网络协议支持：

#### TCP客户端
```loong
import net from "std/net.loong";

val client = net.TcpClient();
client.setTimeout(5000);

if client.connect("example.com", 80) {
    client.send("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    val response = client.recvAll();
    println(response);
    client.close();
}
```

#### UDP客户端
```loong
import net from "std/net.loong";

val udp = net.UdpClient();
udp.open();

// 发送UDP数据
udp.send("127.0.0.1", 8080, "Hello UDP!");

// 绑定端口接收数据
udp.bind(9090);
val data = udp.recv(1024);
println("收到来自 " + data.host + ":" + str(data.port) + " 的数据: " + data.data);

udp.close();
```

#### DNS解析
```loong
import net from "std/net.loong";

val ip = net.dnsResolve("www.example.com");
println("IP地址: " + ip);

val hostname = net.dnsReverse("93.184.216.34");
println("域名: " + hostname);
```

#### 端口检查
```loong
import net from "std/net.loong";

if net.isPortOpen("example.com", 80) {
    println("端口80开放");
}

// 使用NetUtils扫描端口
val utils = net.NetUtils();
val openPorts = utils.scanPorts("192.168.1.1", 1, 100);
println("开放端口: " + str(openPorts));
```

#### TCP服务器
```loong
import net from "std/net.loong";

val server = net.TcpServer();

if server.bind(8080) && server.listen(5) {
    println("服务器启动，监听端口8080");
    
    // 接受客户端连接
    val client = server.acceptClient();
    if client != nil {
        // 接收数据
        val data = client.recv(1024);
        println("收到: " + data);
        
        // 发送响应
        client.send("Hello from server!");
        client.close();
    }
    
    server.close();
}
```

#### 网络协议内置函数
| 函数 | 说明 |
|------|------|
| `tcp_connect(host, port, timeout?)` | 创建TCP连接，返回socket_id |
| `tcp_send(socket_id, data)` | 发送TCP数据 |
| `tcp_recv(socket_id, size?)` | 接收TCP数据 |
| `tcp_recv_exact(socket_id, size)` | 精确接收指定字节数的TCP数据 |
| `tcp_recv_all(socket_id, maxBytes?)` | 接收所有TCP数据 |
| `tcp_close(socket_id)` | 关闭TCP连接 |
| `tcp_connected(socket_id)` | 检查TCP连接状态 |
| `tcp_server_create()` | 创建TCP服务器，返回server_id |
| `tcp_server_bind(server_id, port)` | 绑定端口 |
| `tcp_server_listen(server_id, backlog?)` | 开始监听 |
| `tcp_server_accept(server_id)` | 接受连接，返回client_socket_id |
| `tcp_server_close(server_id)` | 关闭服务器 |
| `udp_open()` | 创建UDP客户端，返回udp_id |
| `udp_send(udp_id, host, port, data)` | 发送UDP数据 |
| `udp_recv(udp_id, size?)` | 接收UDP数据，返回{data,host,port} |
| `udp_bind(udp_id, port)` | 绑定UDP端口 |
| `udp_close(udp_id)` | 关闭UDP |
| `dns_resolve(hostname)` | DNS解析（域名→IP） |
| `dns_reverse(ip)` | 反向DNS解析（IP→域名） |
| `port_check(host, port, timeout?)` | 检查端口是否开放 |

#### 二进制数据处理函数
| 函数 | 说明 |
|------|------|
| `bytes_len(data)` | 获取二进制数据的字节长度 |
| `bytes_get(data, offset)` | 获取指定偏移处的字节值(0-255) |
| `bytes_set(data, offset, value)` | 设置指定偏移处的字节值，返回新数据 |
| `bytes_get_int16(data, offset)` | 读取小端16位整数 |
| `bytes_get_int24(data, offset)` | 读取小端24位整数 |
| `bytes_get_int32(data, offset)` | 读取小端32位整数 |
| `bytes_from_int8(value)` | 将整数转换为1字节字符串 |
| `bytes_from_int16(value)` | 将整数转换为小端2字节字符串 |
| `bytes_from_int24(value)` | 将整数转换为小端3字节字符串 |
| `bytes_from_int32(value)` | 将整数转换为小端4字节字符串 |
| `bytes_to_hex(data)` | 将二进制数据转换为十六进制字符串 |
| `bytes_from_hex(hex)` | 将十六进制字符串转换为二进制数据 |
| `bytes_concat(data1, data2)` | 连接两个二进制数据 |
| `bytes_substr(data, start, len?)` | 截取二进制数据的子串 |
| `bytes_null()` | 返回一个null字节 |
| `bytes_xor(data1, data2)` | 对两个字节数组进行XOR运算 |

#### 哈希函数
| 函数 | 说明 |
|------|------|
| `sha1(data)` | 计算SHA1哈希，返回20字节二进制数据 |

### MySQL 数据库驱动
Loong v1.2.4 提供基于 TCP/Socket 实现的 MySQL 客户端驱动，支持 MySQL 5.7+ 版本。

#### 快速开始
```loong
import mysql;

// 方式一：使用便捷函数
val conn = mysql.connect("localhost", 3306, "root", "password", "mydb");
val result = mysql.query("SELECT * FROM users");
println("行数: " + str(result.rowCount));
mysql.close();

// 方式二：使用 MySQL 类
val db = mysql.MySQL();
db.connect("localhost", 3306, "root", "password", "mydb");
val rs = db.query("SELECT * FROM users LIMIT 10");
db.close();
```

#### MySQL 类方法
```loong
import mysql;

val db = mysql.MySQL();

// 连接数据库
db.connect(host, port, username, password, database);

// 执行查询（SELECT）
val result = db.query("SELECT * FROM users WHERE id = 1");

// 执行非查询语句（INSERT/UPDATE/DELETE）
val execResult = db.execute("INSERT INTO users (name) VALUES ('张三')");
println("影响行数: " + str(execResult.affectedRows));
println("自增ID: " + str(execResult.lastInsertId));

// 切换数据库
db.selectDb("another_database");

// 检测连接
if db.ping() {
    println("连接正常");
}

// 关闭连接
db.close();

// 获取连接信息
db.getServerVersion();    // 服务器版本
db.getConnectionId();     // 连接ID
db.getDatabase();         // 当前数据库
db.isConnected();         // 连接状态
```

#### ResultSet 结果集
```loong
val result = db.query("SELECT id, name, age FROM users");

// 获取列名
println("列名: " + str(result.columns));  // ["id", "name", "age"]

// 获取行数
println("行数: " + str(result.rowCount));

// 遍历结果
val i = 0;
while i < result.rowCount {
    // 按列名获取值
    val id = result.get(i, "id");
    val name = result.get(i, "name");
    val age = result.get(i, "age");
    
    // 或按列索引获取值
    val id2 = result.getValue(i, 0);
    
    println("id=" + id + " name=" + name + " age=" + age);
    i = i + 1;
}

// 对于非查询语句（INSERT/UPDATE/DELETE）
val execResult = db.execute("UPDATE users SET age = 30 WHERE id = 1");
println("影响行数: " + str(execResult.affectedRows));
println("自增ID: " + str(execResult.lastInsertId));
```

#### 完整示例
```loong
import mysql;

try {
    // 连接数据库
    val db = mysql.connect("192.168.1.100", 3306, "root", "password", "testdb");
    println("连接成功! 服务器版本: " + db.getServerVersion());
    
    // 创建表
    db.execute("CREATE TABLE IF NOT EXISTS users (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(100), age INT)");
    
    // 插入数据
    db.execute("INSERT INTO users (name, age) VALUES ('张三', 25)");
    db.execute("INSERT INTO users (name, age) VALUES ('李四', 30)");
    
    // 查询数据
    val result = db.query("SELECT * FROM users");
    println("共 " + str(result.rowCount) + " 条记录");
    
    val i = 0;
    while i < result.rowCount {
        println("id=" + result.get(i, "id") + " name=" + result.get(i, "name") + " age=" + result.get(i, "age"));
        i = i + 1;
    }
    
    // 更新数据
    db.execute("UPDATE users SET age = 26 WHERE name = '张三'");
    
    // 删除数据
    db.execute("DELETE FROM users WHERE name = '李四'");
    
    // 关闭连接
    db.close();
    println("操作完成!");
} catch (e) {
    println("错误: " + e.toString());
}
```

#### MySQL 异常处理
```loong
import mysql

try {
    val db = mysql.connect("localhost", 3306, "root", "wrong_password", "mydb");
} catch (e) {
    // 捕获 MySqlException
    println("错误码: " + str(e.code));
    println("错误信息: " + e.message);
}

try {
    val db = mysql.connect("localhost", 3306, "root", "password", "mydb");
    db.query("SELECT * FROM nonexistent_table");
} catch (e) {
    // 捕获 SQL 错误
    println("SQL错误: " + e.toString());
}
```

#### 注意事项
1. MySQL 驱动基于 TCP/Socket 实现，无需额外依赖
2. 支持 `mysql_native_password` 认证方式
3. 支持 UTF-8 编码，可正确处理中文数据
4. 每次执行新命令时序列号会自动重置
5. 建议使用 try-catch 处理可能的异常

## 包管理器

Loong 包管理器提供简洁的包管理命令：

### 初始化项目
```bash
loong pkg init [目录]    # 创建新项目
```

### 安装包
```bash
loong pkg install <包名>    # 安装指定包
loong pkg install           # 安装所有依赖
```

### 管理包
```bash
loong pkg list              # 列出已安装的包
loong pkg search <关键词>   # 搜索包
loong pkg info <包名>       # 显示包信息
loong pkg update <包名>     # 更新包
loong pkg uninstall <包名>  # 卸载包
loong pkg publish           # 发布当前包
```

### 包配置文件 (loong.pkg)
```
# Loong 包配置文件
name = "mylib"
version = "1.0.0"
description = "A Loong library"
author = "Your Name"
entry = "main.loong"
dependencies = ["json", "crawler"]
```

## 项目结构

```
loong/
├── src/              # 源代码
│   ├── main.cpp      # 入口文件
│   ├── lexer/        # 词法分析器
│   ├── parser/       # 语法分析器
│   ├── interpreter/  # 解释器
│   ├── builtin/      # 内置函数
│   ├── package/      # 包管理器
│   └── utils/        # 工具类
├── std/              # 标准库
├── examples/         # 示例代码
├── CMakeLists.txt    # CMake配置
└── build.bat         # Windows编译脚本
```

## 开发路线

- [x] 词法分析器
- [x] 语法分析器  
- [x] AST节点定义
- [x] 解释器核心
- [x] 变量和作用域
- [x] 函数和闭包
- [x] 循环和控制流
- [x] 列表和字典
- [x] 内置函数
- [x] 模块系统
- [x] 更多标准库
- [x] 异常处理
- [x] 类和对象
- [x] switch/case语法
- [x] 网络请求
- [x] 包管理器
- [ ] 并行处理
- [x] 网络协议（HTTP、TCP、UDP、Socket）
- [x] 数据库驱动（MySQL）
- [x] ASCII码支持
- [x] 字符类型
- [x] 二进制（0b开头）、十六进制（0x开头）和八进制的支持（0开头）
- [x] Socket二进制数据修复
- [x] SHA1哈希函数
- [x] elif多分支解析修复
- [x] 列表/字典/字符串方法（push/pop/keys/values等）
- [x] 匿名函数和 Lambda 表达式

## 许可证

MIT License
