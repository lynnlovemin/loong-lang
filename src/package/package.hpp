#ifndef LOONG_PACKAGE_HPP
#define LOONG_PACKAGE_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace loong {

// 包信息结构
struct PackageInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string repository;
    std::string entry;  // 入口文件
    std::vector<std::string> dependencies;
    std::map<std::string, std::string> dependenciesWithVersion;
};

// 包管理器类
class PackageManager {
public:
    PackageManager();
    
    // 初始化项目
    bool init(const std::string& path = ".");
    
    // 安装包
    bool install(const std::string& packageName, const std::string& version = "latest");
    
    // 卸载包
    bool uninstall(const std::string& packageName);
    
    // 列出已安装的包
    std::vector<PackageInfo> list();
    
    // 搜索包
    std::vector<PackageInfo> search(const std::string& query);
    
    // 获取包信息
    PackageInfo info(const std::string& packageName);
    
    // 更新包
    bool update(const std::string& packageName);
    
    // 安装所有依赖
    bool installDependencies();
    
    // 发布包（本地注册）
    bool publish(const std::string& path = ".");
    
    // 获取错误信息
    std::string getError() const { return error_; }
    
private:
    std::string error_;
    std::string packagesDir_;  // 本地包目录
    std::string registryFile_; // 包注册表文件
    
    // 加载包配置
    bool loadPackageConfig(const std::string& path, PackageInfo& pkg);
    
    // 保存包配置
    bool savePackageConfig(const std::string& path, const PackageInfo& pkg);
    
    // 加载注册表
    std::map<std::string, PackageInfo> loadRegistry();
    
    // 保存注册表
    bool saveRegistry(const std::map<std::string, PackageInfo>& registry);
    
    // 解析版本
    bool parseVersion(const std::string& version, int& major, int& minor, int& patch);
    
    // 比较版本
    int compareVersions(const std::string& v1, const std::string& v2);
    
    // 下载包（从本地仓库或网络）
    bool downloadPackage(const std::string& name, const std::string& version);
    
    // 创建包目录结构
    bool createPackageStructure(const std::string& path);
};

} // namespace loong

#endif // LOONG_PACKAGE_HPP
