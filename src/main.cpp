#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "interpreter/interpreter.hpp"
#include "utils/error.hpp"
#include "package/package.hpp"

#ifdef _WIN32
#include <windows.h>
#include <filesystem>

// 设置Windows控制台支持UTF-8
void setupWindowsConsole() {
    // 设置控制台代码页为UTF-8
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
}

// 获取可执行文件所在目录
std::string getExecutableDirectory() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    return std::string(buffer).substr(0, pos);
}

// 获取文件的绝对路径
std::string getAbsolutePath(const std::string& relativePath) {
    char buffer[MAX_PATH];
    if (GetFullPathNameA(relativePath.c_str(), MAX_PATH, buffer, NULL)) {
        return std::string(buffer);
    }
    return relativePath;
}
#else
#include <limits.h>
#include <unistd.h>

void setupWindowsConsole() {}

// 获取可执行文件所在目录
std::string getExecutableDirectory() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        std::string path(result, count);
        return path.substr(0, path.find_last_of('/'));
    }
    return ".";
}

// 获取文件的绝对路径
std::string getAbsolutePath(const std::string& relativePath) {
    char result[PATH_MAX];
    if (realpath(relativePath.c_str(), result)) {
        return std::string(result);
    }
    return relativePath;
}
#endif

void printUsage(const char* programName) {
    std::cout << "Loong 编程语言 v1.3.2\n";
    std::cout << "用法: " << programName << " [选项] <文件>\n\n";
    std::cout << "选项:\n";
    std::cout << "  -h, --help     显示帮助信息\n";
    std::cout << "  -v, --version  显示版本信息\n";
    std::cout << "  -t, --tokens   输出词法分析结果\n";
    std::cout << "  -a, --ast      输出语法分析结果\n";
    std::cout << "  -c, --code     执行代码字符串\n";
    std::cout << "\n包管理器:\n";
    std::cout << "  pkg init       初始化项目\n";
    std::cout << "  pkg install <包名>   安装包\n";
    std::cout << "  pkg uninstall <包名> 卸载包\n";
    std::cout << "  pkg list       列出已安装的包\n";
    std::cout << "  pkg search <查询>   搜索包\n";
    std::cout << "  pkg info <包名>    显示包信息\n";
    std::cout << "  pkg update <包名>  更新包\n";
    std::cout << "  pkg publish    发布当前包\n";
    std::cout << "\n示例:\n";
    std::cout << "  loong hello.loong      执行脚本文件\n";
    std::cout << "  loong -c \"print(42)\"   执行代码字符串\n";
    std::cout << "  loong pkg install json 安装json包\n";
}

void printVersion() {
    std::cout << "Loong 编程语言 v1.3.2\n";
    std::cout << "寓意: 龙\n";
    std::cout << "解释型编程语言，采用C++17开发\n";
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void run(const std::string& source, bool showTokens = false, bool showAst = false, 
         const std::string& sourceFile = "", const std::string& execPath = "") {
    // 词法分析
    loong::Lexer lexer(source);
    auto tokens = lexer.tokenize();
    
    if (showTokens) {
        std::cout << "=== Tokens ===\n";
        for (const auto& token : tokens) {
            std::cout << loong::Token::typeToString(token.type) 
                      << " [" << token.value << "] "
                      << "(" << token.line << ":" << token.column << ")\n";
        }
        std::cout << std::endl;
    }
    
    // 语法分析
    loong::Parser parser(tokens);
    loong::Program program = parser.parse();
    
    if (parser.hadError()) {
        std::cerr << "语法错误: " << parser.getErrorMessage() << std::endl;
        return;
    }
    
    if (showAst) {
        std::cout << "=== AST 解析成功 ===\n";
        std::cout << "语句数量: " << program.statements.size() << std::endl;
    }
    
    // 解释执行
    try {
        loong::Interpreter interpreter;
        if (!sourceFile.empty()) {
            interpreter.setSourceFile(sourceFile);
        }
        if (!execPath.empty()) {
            interpreter.setExecutablePath(execPath);
        }
        interpreter.interpret(program);
    } catch (const loong::LoongException& e) {
        std::cerr << "未捕获的异常: " << e.value.toString() << std::endl;
    }
}

void runFile(const std::string& path, bool showTokens = false, bool showAst = false) {
    std::string absolutePath = getAbsolutePath(path);
    std::string source = readFile(path);
    std::string execDir = getExecutableDirectory();
    run(source, showTokens, showAst, absolutePath, execDir);
}

void runRepl() {
    std::cout << "Loong 交互模式 (输入 'exit' 退出)\n\n";
    
    loong::Interpreter interpreter;
    interpreter.setExecutablePath(getExecutableDirectory());
    std::string line;
    
    while (true) {
        std::cout << "loong> ";
        std::getline(std::cin, line);
        
        if (line == "exit" || line == "quit") {
            break;
        }
        
        if (line.empty()) {
            continue;
        }
        
        try {
            // 词法分析
            loong::Lexer lexer(line);
            auto tokens = lexer.tokenize();
            
            // 语法分析
            loong::Parser parser(tokens);
            loong::Program program = parser.parse();
            
            if (parser.hadError()) {
                std::cerr << "错误: " << parser.getErrorMessage() << std::endl;
                continue;
            }
            
            // 执行
            interpreter.interpret(program);
        } catch (const loong::LoongException& e) {
            std::cerr << "异常: " << e.value.toString() << std::endl;
        } catch (const loong::LoongError& e) {
            std::cerr << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "错误: " << e.what() << std::endl;
        }
    }
    
    std::cout << "再见！\n";
}

// 包管理器命令处理
int handlePackageCommand(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "错误: 缺少pkg子命令\n";
        std::cout << "用法: loong pkg <命令> [参数]\n";
        std::cout << "命令: init, install, uninstall, list, search, info, update, publish\n";
        return 1;
    }
    
    std::string subCommand = argv[2];
    loong::PackageManager pkgMgr;
    
    if (subCommand == "init") {
        std::string path = argc > 3 ? argv[3] : ".";
        if (pkgMgr.init(path)) {
            std::cout << "项目已初始化\n";
            std::cout << "创建了 loong.pkg 配置文件\n";
            return 0;
        } else {
            std::cerr << "错误: " << pkgMgr.getError() << "\n";
            return 1;
        }
    }
    else if (subCommand == "install") {
        if (argc < 4) {
            // 安装所有依赖
            if (pkgMgr.installDependencies()) {
                return 0;
            }
            std::cerr << "错误: " << pkgMgr.getError() << "\n";
            return 1;
        }
        
        std::string packageName = argv[3];
        std::string version = argc > 4 ? argv[4] : "latest";
        
        if (pkgMgr.install(packageName, version)) {
            return 0;
        }
        std::cerr << "错误: " << pkgMgr.getError() << "\n";
        return 1;
    }
    else if (subCommand == "uninstall") {
        if (argc < 4) {
            std::cerr << "错误: 请指定要卸载的包名\n";
            return 1;
        }
        
        if (pkgMgr.uninstall(argv[3])) {
            return 0;
        }
        std::cerr << "错误: " << pkgMgr.getError() << "\n";
        return 1;
    }
    else if (subCommand == "list") {
        auto packages = pkgMgr.list();
        std::cout << "已安装的包:\n";
        if (packages.empty()) {
            std::cout << "  (无)\n";
        } else {
            for (const auto& pkg : packages) {
                std::cout << "  - " << pkg.name << " v" << pkg.version;
                if (!pkg.description.empty()) {
                    std::cout << " - " << pkg.description;
                }
                std::cout << "\n";
            }
        }
        return 0;
    }
    else if (subCommand == "search") {
        if (argc < 4) {
            std::cerr << "错误: 请输入搜索关键词\n";
            return 1;
        }
        
        auto results = pkgMgr.search(argv[3]);
        std::cout << "搜索结果:\n";
        if (results.empty()) {
            std::cout << "  未找到匹配的包\n";
        } else {
            for (const auto& pkg : results) {
                std::cout << "  - " << pkg.name << " v" << pkg.version;
                if (!pkg.description.empty()) {
                    std::cout << " - " << pkg.description;
                }
                std::cout << "\n";
            }
        }
        return 0;
    }
    else if (subCommand == "info") {
        if (argc < 4) {
            std::cerr << "错误: 请指定包名\n";
            return 1;
        }
        
        auto pkg = pkgMgr.info(argv[3]);
        if (pkg.name.empty()) {
            std::cerr << "错误: 未找到包\n";
            return 1;
        }
        
        std::cout << "名称: " << pkg.name << "\n";
        std::cout << "版本: " << pkg.version << "\n";
        std::cout << "描述: " << pkg.description << "\n";
        std::cout << "作者: " << pkg.author << "\n";
        std::cout << "入口: " << pkg.entry << "\n";
        if (!pkg.dependencies.empty()) {
            std::cout << "依赖:\n";
            for (const auto& dep : pkg.dependencies) {
                std::cout << "  - " << dep << "\n";
            }
        }
        return 0;
    }
    else if (subCommand == "update") {
        if (argc < 4) {
            std::cerr << "错误: 请指定要更新的包名\n";
            return 1;
        }
        
        if (pkgMgr.update(argv[3])) {
            return 0;
        }
        std::cerr << "错误: " << pkgMgr.getError() << "\n";
        return 1;
    }
    else if (subCommand == "publish") {
        std::string path = argc > 3 ? argv[3] : ".";
        if (pkgMgr.publish(path)) {
            return 0;
        }
        std::cerr << "错误: " << pkgMgr.getError() << "\n";
        return 1;
    }
    else {
        std::cerr << "错误: 未知的pkg命令: " << subCommand << "\n";
        return 1;
    }
}

int main(int argc, char* argv[]) {
    // 设置控制台UTF-8编码
    setupWindowsConsole();
    
    if (argc < 2) {
        runRepl();
        return 0;
    }
    
    std::string arg1 = argv[1];
    
    // 包管理器命令
    if (arg1 == "pkg") {
        return handlePackageCommand(argc, argv);
    }
    
    // 帮助
    if (arg1 == "-h" || arg1 == "--help") {
        printUsage(argv[0]);
        return 0;
    }
    
    // 版本
    if (arg1 == "-v" || arg1 == "--version") {
        printVersion();
        return 0;
    }
    
    // 执行代码字符串
    if (arg1 == "-c" || arg1 == "--code") {
        if (argc < 3) {
            std::cerr << "错误: 缺少代码参数\n";
            return 1;
        }
        
        try {
            run(argv[2], false, false, "", getExecutableDirectory());
        } catch (const loong::LoongException& e) {
            std::cerr << "未捕获的异常: " << e.value.toString() << std::endl;
            return 1;
        } catch (const loong::LoongError& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        } catch (const std::exception& e) {
            std::cerr << "错误: " << e.what() << std::endl;
            return 1;
        }
        return 0;
    }
    
    // 显示Tokens
    bool showTokens = false;
    bool showAst = false;
    std::string filePath;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-t" || arg == "--tokens") {
            showTokens = true;
        } else if (arg == "-a" || arg == "--ast") {
            showAst = true;
        } else if (arg[0] != '-') {
            filePath = arg;
        }
    }
    
    if (filePath.empty()) {
        std::cerr << "错误: 缺少文件参数\n";
        printUsage(argv[0]);
        return 1;
    }
    
    try {
        runFile(filePath, showTokens, showAst);
    } catch (const loong::LoongException& e) {
        std::cerr << "未捕获的异常: " << e.value.toString() << std::endl;
        return 1;
    } catch (const loong::LoongError& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
