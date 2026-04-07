# Loong 编程语言 v1.0.0

## 简介

**Loong**（龙）是一款解释型编程语言，寓意为龙。采用C++17开发，具有简洁优雅的语法。

## 下载安装

📥 **[下载 Windows 安装包](loong-1.0.0-setup.exe)** (v1.0.0)

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

// 导入指定路径的模块
import mylib from "lib/mylib.loong";
println(mylib.greet("Loong"));

// 导入网络爬虫库
import crawler from "std/crawler.loong";
val resp = crawler.fetch("http://example.com");
```

#### 模块寻址规则

当使用 `import 模块名` 导入模块时，Loong 会按以下优先级顺序查找模块文件：

1. **源码所在目录** - 从当前执行的 `.loong` 文件所在目录开始，向上逐级查找父目录
2. **loong.exe 所在目录** - 从解释器所在目录开始，向上逐级查找父目录（包括 `std/` 子目录）
3. **当前工作目录** - 从命令行执行的目录开始，向上逐级查找父目录
4. **默认路径** - `std/模块名.loong`

在每个目录中，会尝试以下路径组合：
- `模块名.loong`
- `模块名/模块名.loong`
- `lib/模块名.loong`
- `模块名/main.loong`

当使用 `import 模块名 from "路径"` 指定路径时：
- 绝对路径直接使用
- 相对路径相对于当前源文件所在目录解析

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
- `int(x)` - 转整数

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
- [x] 网络请求
- [x] 包管理器
- [ ] 并行处理

## 许可证

MIT License
