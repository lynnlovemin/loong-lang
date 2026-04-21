#ifndef LOONG_ENVIRONMENT_HPP
#define LOONG_ENVIRONMENT_HPP

#include "value.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace loong {

class Environment : public std::enable_shared_from_this<Environment> {
private:
    std::unordered_map<std::string, LoongValue> values_;
    std::unordered_set<std::string> constants_;
    std::unordered_map<std::string, std::string> types_;  // 变量类型注解
    std::shared_ptr<Environment> enclosing_;
    mutable std::mutex mutex_;

public:
    Environment() : enclosing_(nullptr) {}
    
    explicit Environment(std::shared_ptr<Environment> enclosing)
        : enclosing_(enclosing) {}
    
    std::mutex& getMutex() { return mutex_; }
    
    // 定义变量
    void define(const std::string& name, const LoongValue& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_[name] = value;
    }
    
    // 定义常量
    void defineConst(const std::string& name, const LoongValue& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_[name] = value;
        constants_.insert(name);
    }
    
    // 定义带类型注解的变量
    void defineWithType(const std::string& name, const LoongValue& value, const std::string& typeName) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_[name] = value;
        if (!typeName.empty()) {
            types_[name] = typeName;
        }
    }
    
    // 定义带类型注解的常量
    void defineConstWithType(const std::string& name, const LoongValue& value, const std::string& typeName) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_[name] = value;
        constants_.insert(name);
        if (!typeName.empty()) {
            types_[name] = typeName;
        }
    }
    
    // 获取变量的类型注解
    std::string getType(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = types_.find(name);
        if (it != types_.end()) {
            return it->second;
        }
        if (enclosing_) {
            return enclosing_->getType(name);
        }
        return "";
    }
    
    // 检查是否为常量
    bool isConst(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (constants_.find(name) != constants_.end()) {
            return true;
        }
        if (enclosing_) {
            return enclosing_->isConst(name);
        }
        return false;
    }
    
    // 获取变量
    LoongValue get(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = values_.find(name);
        if (it != values_.end()) {
            return it->second;
        }
        
        if (enclosing_) {
            return enclosing_->get(name);
        }
        
        throw std::runtime_error("未定义的变量: " + name);
    }
    
    // 检查变量是否存在
    bool exists(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (values_.find(name) != values_.end()) {
            return true;
        }
        if (enclosing_) {
            return enclosing_->exists(name);
        }
        return false;
    }
    
    // 在当前作用域检查变量是否存在
    bool existsLocal(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return values_.find(name) != values_.end();
    }
    
    // 赋值变量
    void assign(const std::string& name, const LoongValue& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = values_.find(name);
        if (it != values_.end()) {
            it->second = value;
            return;
        }
        
        if (enclosing_) {
            enclosing_->assign(name, value);
            return;
        }
        
        throw std::runtime_error("未定义的变量: " + name);
    }
    
    // 获取外层环境
    std::shared_ptr<Environment> getEnclosing() const {
        return enclosing_;
    }
    
    // 创建子环境
    std::shared_ptr<Environment> createChild() {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::make_shared<Environment>(shared_from_this());
    }
    
    // 获取所有变量（用于调试）
    const std::unordered_map<std::string, LoongValue>& getValues() const {
        return values_;
    }
};

} // namespace loong

#endif // LOONG_ENVIRONMENT_HPP
