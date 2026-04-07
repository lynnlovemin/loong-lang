#include "value.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <limits>

namespace loong {

// 辅助函数：格式化数字，避免科学计数法
static std::string formatNumber(double value) {
    // 检查是否为整数
    if (std::floor(value) == value && 
        value >= -static_cast<double>(std::numeric_limits<long long>::max()) &&
        value <= static_cast<double>(std::numeric_limits<long long>::max())) {
        // 使用整数格式输出，避免科学计数法
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << value;
        return oss.str();
    }
    
    // 浮点数处理：先尝试默认格式
    std::ostringstream oss_default;
    oss_default << std::setprecision(15) << value;
    std::string defaultResult = oss_default.str();
    
    // 如果默认格式没有使用科学计数法，直接使用
    if (defaultResult.find('e') == std::string::npos && 
        defaultResult.find('E') == std::string::npos) {
        return defaultResult;
    }
    
    // 对于使用科学计数法的数，转换为固定格式
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(15) << value;
    std::string result = oss.str();
    
    // 去除尾部的0
    if (result.find('.') != std::string::npos) {
        size_t lastNonZero = result.find_last_not_of('0');
        if (lastNonZero != std::string::npos) {
            result = result.substr(0, lastNonZero + 1);
            if (!result.empty() && result.back() == '.') {
                result.pop_back();
            }
        }
    }
    
    return result;
}

// LoongClass实现
LoongClass::LoongClass(const std::string& n, const std::string& sc)
    : name(n), superClass(sc) {}

LoongValue LoongClass::findMethod(const std::string& methodName) {
    auto it = methods.find(methodName);
    if (it != methods.end()) {
        return it->second;
    }
    // 父类查找需要在运行时处理
    return LoongValue::nil();
}

// LoongValue::toString 实现
std::string LoongValue::toString() const {
    switch (type) {
        case ValueType::NIL:
            return "nil";
        case ValueType::BOOL:
            return boolValue ? "true" : "false";
        case ValueType::NUMBER:
            return formatNumber(numberValue);
        case ValueType::BIGINT:
            return bigintValue.toString();
        case ValueType::STRING:
            return stringValue;
        case ValueType::LIST: {
            std::ostringstream oss;
            oss << "[";
            for (size_t i = 0; i < listValue.size(); i++) {
                if (i > 0) oss << ", ";
                if (listValue[i].type == ValueType::STRING) {
                    oss << "\"" << listValue[i].stringValue << "\"";
                } else {
                    oss << listValue[i].toString();
                }
            }
            oss << "]";
            return oss.str();
        }
        case ValueType::DICT: {
            std::ostringstream oss;
            oss << "{";
            bool first = true;
            for (const auto& pair : dictValue) {
                if (!first) oss << ", ";
                first = false;
                oss << "\"" << pair.first << "\": ";
                if (pair.second.type == ValueType::STRING) {
                    oss << "\"" << pair.second.stringValue << "\"";
                } else {
                    oss << pair.second.toString();
                }
            }
            oss << "}";
            return oss.str();
        }
        case ValueType::FUNCTION:
            return "<function " + userFunc->name + ">";
        case ValueType::BUILTIN_FUNCTION:
            return "<builtin function>";
        case ValueType::CLASS:
            return "<class " + classValue->name + ">";
        case ValueType::INSTANCE:
            return "<instance of " + instanceValue->klass->name + ">";
        case ValueType::BOUND_METHOD:
            return "<method " + boundMethodValue->method->name + ">";
        default:
            return "<unknown>";
    }
}

} // namespace loong
