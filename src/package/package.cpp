#include "package.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <regex>

namespace fs = std::filesystem;

namespace loong {

PackageManager::PackageManager() {
    // 设置包目录
#ifdef _WIN32
    char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        packagesDir_ = std::string(userProfile) + "\\.loong\\packages";
        registryFile_ = std::string(userProfile) + "\\.loong\\registry.json";
    } else {
        packagesDir_ = ".loong\\packages";
        registryFile_ = ".loong\\registry.json";
    }
#else
    char* homeDir = std::getenv("HOME");
    if (homeDir) {
        packagesDir_ = std::string(homeDir) + "/.loong/packages";
        registryFile_ = std::string(homeDir) + "/.loong/registry.json";
    } else {
        packagesDir_ = ".loong/packages";
        registryFile_ = ".loong/registry.json";
    }
#endif
    
    // 确保目录存在
    fs::create_directories(packagesDir_);
}

bool PackageManager::init(const std::string& path) {
    std::string targetPath = path.empty() ? "." : path;
    
    // 创建项目结构
    if (!createPackageStructure(targetPath)) {
        return false;
    }
    
    // 创建默认的 loong.pkg 配置文件
    PackageInfo pkg;
    pkg.name = fs::path(targetPath).filename().string();
    pkg.version = "1.0.0";
    pkg.description = "A Loong package";
    pkg.author = "";
    pkg.entry = "main.loong";
    
    std::string configPath = targetPath + "/loong.pkg";
    
    // 检查是否已存在
    if (fs::exists(configPath)) {
        error_ = "loong.pkg 已存在";
        return false;
    }
    
    return savePackageConfig(configPath, pkg);
}

bool PackageManager::install(const std::string& packageName, const std::string& version) {
    // 检查本地注册表
    auto registry = loadRegistry();
    
    if (registry.find(packageName) == registry.end()) {
        error_ = "未找到包: " + packageName;
        return false;
    }
    
    PackageInfo pkgInfo = registry[packageName];
    
    // 检查版本
    if (version != "latest" && !pkgInfo.version.empty()) {
        if (compareVersions(version, pkgInfo.version) > 0) {
            error_ = "版本 " + version + " 不可用，最新版本: " + pkgInfo.version;
            return false;
        }
    }
    
    // 创建包目录
    std::string pkgDir = packagesDir_ + "/" + packageName;
    fs::create_directories(pkgDir);
    
    // 复制包文件（从注册表中的路径）
    if (!pkgInfo.repository.empty() && fs::exists(pkgInfo.repository)) {
        try {
            fs::copy(pkgInfo.repository, pkgDir, 
                    fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        } catch (const std::exception& e) {
            error_ = "复制包文件失败: " + std::string(e.what());
            return false;
        }
    }
    
    // 更新项目的依赖
    std::string projectPkgPath = "./loong.pkg";
    if (fs::exists(projectPkgPath)) {
        PackageInfo projectPkg;
        if (loadPackageConfig(projectPkgPath, projectPkg)) {
            projectPkg.dependencies.push_back(packageName);
            projectPkg.dependenciesWithVersion[packageName] = pkgInfo.version;
            savePackageConfig(projectPkgPath, projectPkg);
        }
    }
    
    std::cout << "已安装: " << packageName << " v" << pkgInfo.version << std::endl;
    return true;
}

bool PackageManager::uninstall(const std::string& packageName) {
    std::string pkgDir = packagesDir_ + "/" + packageName;
    
    if (!fs::exists(pkgDir)) {
        error_ = "包未安装: " + packageName;
        return false;
    }
    
    try {
        fs::remove_all(pkgDir);
    } catch (const std::exception& e) {
        error_ = "删除包失败: " + std::string(e.what());
        return false;
    }
    
    // 更新项目的依赖
    std::string projectPkgPath = "./loong.pkg";
    if (fs::exists(projectPkgPath)) {
        PackageInfo projectPkg;
        if (loadPackageConfig(projectPkgPath, projectPkg)) {
            // 从依赖列表中移除
            auto it = std::find(projectPkg.dependencies.begin(), 
                               projectPkg.dependencies.end(), packageName);
            if (it != projectPkg.dependencies.end()) {
                projectPkg.dependencies.erase(it);
            }
            projectPkg.dependenciesWithVersion.erase(packageName);
            savePackageConfig(projectPkgPath, projectPkg);
        }
    }
    
    std::cout << "已卸载: " << packageName << std::endl;
    return true;
}

std::vector<PackageInfo> PackageManager::list() {
    std::vector<PackageInfo> packages;
    
    if (!fs::exists(packagesDir_)) {
        return packages;
    }
    
    for (const auto& entry : fs::directory_iterator(packagesDir_)) {
        if (entry.is_directory()) {
            std::string pkgName = entry.path().filename().string();
            PackageInfo pkg = info(pkgName);
            if (!pkg.name.empty()) {
                packages.push_back(pkg);
            }
        }
    }
    
    return packages;
}

std::vector<PackageInfo> PackageManager::search(const std::string& query) {
    std::vector<PackageInfo> results;
    auto registry = loadRegistry();
    
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    for (const auto& [name, pkg] : registry) {
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        std::string lowerDesc = pkg.description;
        std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);
        
        if (lowerName.find(lowerQuery) != std::string::npos ||
            lowerDesc.find(lowerQuery) != std::string::npos) {
            results.push_back(pkg);
        }
    }
    
    return results;
}

PackageInfo PackageManager::info(const std::string& packageName) {
    PackageInfo pkg;
    pkg.name = packageName;
    
    // 先检查已安装的包
    std::string pkgDir = packagesDir_ + "/" + packageName;
    std::string configPath = pkgDir + "/loong.pkg";
    
    if (fs::exists(configPath)) {
        loadPackageConfig(configPath, pkg);
        return pkg;
    }
    
    // 检查注册表
    auto registry = loadRegistry();
    if (registry.find(packageName) != registry.end()) {
        return registry[packageName];
    }
    
    return pkg;
}

bool PackageManager::update(const std::string& packageName) {
    // 先卸载再安装最新版本
    uninstall(packageName);
    return install(packageName, "latest");
}

bool PackageManager::installDependencies() {
    std::string projectPkgPath = "./loong.pkg";
    
    if (!fs::exists(projectPkgPath)) {
        error_ = "未找到 loong.pkg 文件";
        return false;
    }
    
    PackageInfo projectPkg;
    if (!loadPackageConfig(projectPkgPath, projectPkg)) {
        return false;
    }
    
    std::cout << "安装依赖..." << std::endl;
    
    for (const auto& dep : projectPkg.dependencies) {
        std::string version = "latest";
        if (projectPkg.dependenciesWithVersion.find(dep) != 
            projectPkg.dependenciesWithVersion.end()) {
            version = projectPkg.dependenciesWithVersion[dep];
        }
        
        std::cout << "安装: " << dep << "@" << version << std::endl;
        if (!install(dep, version)) {
            std::cerr << "警告: 无法安装 " << dep << ": " << error_ << std::endl;
        }
    }
    
    return true;
}

bool PackageManager::publish(const std::string& path) {
    std::string targetPath = path.empty() ? "." : path;
    std::string configPath = targetPath + "/loong.pkg";
    
    if (!fs::exists(configPath)) {
        error_ = "未找到 loong.pkg 文件";
        return false;
    }
    
    PackageInfo pkg;
    if (!loadPackageConfig(configPath, pkg)) {
        return false;
    }
    
    // 加载注册表
    auto registry = loadRegistry();
    
    // 添加或更新包信息
    pkg.repository = fs::absolute(targetPath).string();
    registry[pkg.name] = pkg;
    
    // 保存注册表
    if (!saveRegistry(registry)) {
        return false;
    }
    
    std::cout << "已发布: " << pkg.name << " v" << pkg.version << std::endl;
    return true;
}

bool PackageManager::loadPackageConfig(const std::string& path, PackageInfo& pkg) {
    std::ifstream file(path);
    if (!file.is_open()) {
        error_ = "无法打开配置文件: " + path;
        return false;
    }
    
    // 解析简化的配置文件格式
    // 格式：每行 key = value
    std::string line;
    while (std::getline(file, line)) {
        // 跳过注释和空行
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // 去除空白
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t\""));
        value.erase(value.find_last_not_of(" \t\"") + 1);
        
        if (key == "name") pkg.name = value;
        else if (key == "version") pkg.version = value;
        else if (key == "description") pkg.description = value;
        else if (key == "author") pkg.author = value;
        else if (key == "repository") pkg.repository = value;
        else if (key == "entry") pkg.entry = value;
        else if (key == "dependencies") {
            // 解析依赖列表: ["pkg1", "pkg2"]
            if (value.front() == '[' && value.back() == ']') {
                value = value.substr(1, value.length() - 2);
                std::stringstream ss(value);
                std::string dep;
                while (std::getline(ss, dep, ',')) {
                    dep.erase(0, dep.find_first_not_of(" \t\""));
                    dep.erase(dep.find_last_not_of(" \t\"") + 1);
                    if (!dep.empty()) {
                        pkg.dependencies.push_back(dep);
                    }
                }
            }
        }
    }
    
    return true;
}

bool PackageManager::savePackageConfig(const std::string& path, const PackageInfo& pkg) {
    std::ofstream file(path);
    if (!file.is_open()) {
        error_ = "无法创建配置文件: " + path;
        return false;
    }
    
    file << "# Loong 包配置文件\n";
    file << "name = \"" << pkg.name << "\"\n";
    file << "version = \"" << pkg.version << "\"\n";
    file << "description = \"" << pkg.description << "\"\n";
    file << "author = \"" << pkg.author << "\"\n";
    file << "entry = \"" << pkg.entry << "\"\n";
    
    if (!pkg.repository.empty()) {
        file << "repository = \"" << pkg.repository << "\"\n";
    }
    
    if (!pkg.dependencies.empty()) {
        file << "dependencies = [";
        for (size_t i = 0; i < pkg.dependencies.size(); i++) {
            if (i > 0) file << ", ";
            file << "\"" << pkg.dependencies[i] << "\"";
        }
        file << "]\n";
    }
    
    return true;
}

std::map<std::string, PackageInfo> PackageManager::loadRegistry() {
    std::map<std::string, PackageInfo> registry;
    
    if (!fs::exists(registryFile_)) {
        return registry;
    }
    
    std::ifstream file(registryFile_);
    std::string line;
    PackageInfo currentPkg;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        if (line.find("---") == 0) {
            if (!currentPkg.name.empty()) {
                registry[currentPkg.name] = currentPkg;
            }
            currentPkg = PackageInfo();
            continue;
        }
        
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t\""));
        value.erase(value.find_last_not_of(" \t\"") + 1);
        
        if (key == "name") currentPkg.name = value;
        else if (key == "version") currentPkg.version = value;
        else if (key == "description") currentPkg.description = value;
        else if (key == "author") currentPkg.author = value;
        else if (key == "repository") currentPkg.repository = value;
        else if (key == "entry") currentPkg.entry = value;
    }
    
    if (!currentPkg.name.empty()) {
        registry[currentPkg.name] = currentPkg;
    }
    
    return registry;
}

bool PackageManager::saveRegistry(const std::map<std::string, PackageInfo>& registry) {
    // 确保目录存在
    fs::path p(registryFile_);
    fs::create_directories(p.parent_path());
    
    std::ofstream file(registryFile_);
    if (!file.is_open()) {
        error_ = "无法保存注册表";
        return false;
    }
    
    file << "# Loong 包注册表\n\n";
    
    for (const auto& [name, pkg] : registry) {
        file << "---\n";
        file << "name = \"" << pkg.name << "\"\n";
        file << "version = \"" << pkg.version << "\"\n";
        file << "description = \"" << pkg.description << "\"\n";
        file << "author = \"" << pkg.author << "\"\n";
        file << "repository = \"" << pkg.repository << "\"\n";
        file << "entry = \"" << pkg.entry << "\"\n\n";
    }
    
    return true;
}

bool PackageManager::parseVersion(const std::string& version, int& major, int& minor, int& patch) {
    major = minor = patch = 0;
    
    std::regex versionRegex("(\\d+)(?:\\.(\\d+))?(?:\\.(\\d+))?");
    std::smatch match;
    
    if (std::regex_match(version, match, versionRegex)) {
        major = std::stoi(match[1].str());
        if (match[2].matched) minor = std::stoi(match[2].str());
        if (match[3].matched) patch = std::stoi(match[3].str());
        return true;
    }
    
    return false;
}

int PackageManager::compareVersions(const std::string& v1, const std::string& v2) {
    int maj1, min1, pat1, maj2, min2, pat2;
    parseVersion(v1, maj1, min1, pat1);
    parseVersion(v2, maj2, min2, pat2);
    
    if (maj1 != maj2) return maj1 > maj2 ? 1 : -1;
    if (min1 != min2) return min1 > min2 ? 1 : -1;
    if (pat1 != pat2) return pat1 > pat2 ? 1 : -1;
    return 0;
}

bool PackageManager::createPackageStructure(const std::string& path) {
    try {
        fs::create_directories(path + "/src");
        fs::create_directories(path + "/lib");
        fs::create_directories(path + "/tests");
        return true;
    } catch (const std::exception& e) {
        error_ = "无法创建目录结构: " + std::string(e.what());
        return false;
    }
}

} // namespace loong
