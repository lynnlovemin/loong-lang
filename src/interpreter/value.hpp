#ifndef LOONG_VALUE_HPP
#define LOONG_VALUE_HPP

#include "bigint.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <sstream>
#include <iomanip>

namespace loong {

// 前向声明
class Environment;
class Stmt;
class LoongClass;
struct LoongInstance;
struct BoundMethod;

// 表达式指针类型（前向声明兼容）
using ExprPtr = std::shared_ptr<class Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

// 函数类型
using LoongFunction = std::function<struct LoongValue(const std::vector<struct LoongValue>&)>;

// 默认参数信息结构
struct DefaultParamInfo {
    std::string name;
    std::shared_ptr<class Expr> defaultValue;
    
    DefaultParamInfo(const std::string& n, std::shared_ptr<class Expr> v)
        : name(n), defaultValue(v) {}
};

// 用户定义函数信息
struct UserFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<DefaultParamInfo> defaultParams;
    std::shared_ptr<Environment> closure;
    std::vector<StmtPtr> body;
};

// 值类型枚举
enum class ValueType {
    NIL,
    BOOL,
    NUMBER,
    BIGINT,  // 大整数
    STRING,
    LIST,
    DICT,
    FUNCTION,
    BUILTIN_FUNCTION,
    CLASS,
    INSTANCE,
    BOUND_METHOD
};

// LoongValue 类型定义
struct LoongValue {
    ValueType type;
    
    // 使用 variant 存储不同类型的值
    bool boolValue;
    double numberValue;
    BigInteger bigintValue;  // 大整数
    std::string stringValue;
    std::vector<LoongValue> listValue;
    std::map<std::string, LoongValue> dictValue;
    LoongFunction builtinFunc;
    std::shared_ptr<UserFunction> userFunc;
    std::shared_ptr<LoongClass> classValue;
    std::shared_ptr<LoongInstance> instanceValue;
    std::shared_ptr<BoundMethod> boundMethodValue;  // 改名避免与函数冲突
    
    // 构造函数
    LoongValue() : type(ValueType::NIL), boolValue(false), numberValue(0), bigintValue(0) {}
    
    // 静态工厂方法
    static LoongValue nil() {
        LoongValue v;
        v.type = ValueType::NIL;
        return v;
    }
    
    static LoongValue boolean(bool value) {
        LoongValue v;
        v.type = ValueType::BOOL;
        v.boolValue = value;
        return v;
    }
    
    static LoongValue number(double value) {
        LoongValue v;
        v.type = ValueType::NUMBER;
        v.numberValue = value;
        return v;
    }
    
    static LoongValue bigint(const BigInteger& value) {
        LoongValue v;
        v.type = ValueType::BIGINT;
        v.bigintValue = value;
        return v;
    }
    
    static LoongValue bigint(long long value) {
        LoongValue v;
        v.type = ValueType::BIGINT;
        v.bigintValue = BigInteger(value);
        return v;
    }
    
    static LoongValue bigint(const std::string& value) {
        LoongValue v;
        v.type = ValueType::BIGINT;
        v.bigintValue = BigInteger(value);
        return v;
    }
    
    static LoongValue string(const std::string& value) {
        LoongValue v;
        v.type = ValueType::STRING;
        v.stringValue = value;
        return v;
    }
    
    static LoongValue list(const std::vector<LoongValue>& value) {
        LoongValue v;
        v.type = ValueType::LIST;
        v.listValue = value;
        return v;
    }
    
    static LoongValue dict(const std::map<std::string, LoongValue>& value) {
        LoongValue v;
        v.type = ValueType::DICT;
        v.dictValue = value;
        return v;
    }
    
    static LoongValue builtinFunction(const LoongFunction& func) {
        LoongValue v;
        v.type = ValueType::BUILTIN_FUNCTION;
        v.builtinFunc = func;
        return v;
    }
    
    static LoongValue userFunction(std::shared_ptr<UserFunction> func) {
        LoongValue v;
        v.type = ValueType::FUNCTION;
        v.userFunc = func;
        return v;
    }
    
    static LoongValue classVal(std::shared_ptr<LoongClass> cls) {
        LoongValue v;
        v.type = ValueType::CLASS;
        v.classValue = cls;
        return v;
    }
    
    static LoongValue instance(std::shared_ptr<LoongInstance> inst) {
        LoongValue v;
        v.type = ValueType::INSTANCE;
        v.instanceValue = inst;
        return v;
    }
    
    static LoongValue boundMethod(std::shared_ptr<BoundMethod> bm) {
        LoongValue v;
        v.type = ValueType::BOUND_METHOD;
        v.boundMethodValue = bm;
        return v;
    }
    
    // 类型判断
    bool isNil() const { return type == ValueType::NIL; }
    bool isBool() const { return type == ValueType::BOOL; }
    bool isNumber() const { return type == ValueType::NUMBER; }
    bool isBigint() const { return type == ValueType::BIGINT; }
    bool isNumeric() const { return type == ValueType::NUMBER || type == ValueType::BIGINT; }
    bool isString() const { return type == ValueType::STRING; }
    bool isList() const { return type == ValueType::LIST; }
    bool isDict() const { return type == ValueType::DICT; }
    bool isFunction() const { return type == ValueType::FUNCTION || type == ValueType::BUILTIN_FUNCTION; }
    bool isBuiltinFunction() const { return type == ValueType::BUILTIN_FUNCTION; }
    bool isUserFunction() const { return type == ValueType::FUNCTION; }
    bool isClass() const { return type == ValueType::CLASS; }
    bool isInstance() const { return type == ValueType::INSTANCE; }
    bool isBoundMethod() const { return type == ValueType::BOUND_METHOD; }
    
    // 真值判断
    bool isTruthy() const {
        switch (type) {
            case ValueType::NIL: return false;
            case ValueType::BOOL: return boolValue;
            case ValueType::NUMBER: return numberValue != 0;
            case ValueType::BIGINT: return !bigintValue.isZero();
            case ValueType::STRING: return !stringValue.empty();
            case ValueType::LIST: return !listValue.empty();
            case ValueType::DICT: return !dictValue.empty();
            case ValueType::FUNCTION:
            case ValueType::BUILTIN_FUNCTION:
            case ValueType::CLASS:
            case ValueType::INSTANCE:
            case ValueType::BOUND_METHOD:
                return true;
            default: return false;
        }
    }
    
    // 类型名称
    std::string typeName() const {
        switch (type) {
            case ValueType::NIL: return "nil";
            case ValueType::BOOL: return "bool";
            case ValueType::NUMBER: return "number";
            case ValueType::BIGINT: return "bigint";
            case ValueType::STRING: return "string";
            case ValueType::LIST: return "list";
            case ValueType::DICT: return "dict";
            case ValueType::FUNCTION:
            case ValueType::BUILTIN_FUNCTION:
                return "function";
            case ValueType::CLASS:
                return "class";
            case ValueType::INSTANCE:
                return "instance";
            case ValueType::BOUND_METHOD:
                return "method";
            default: return "unknown";
        }
    }
    
    // 转换为字符串表示
    std::string toString() const;
    
    // 调试输出（带引号的字符串）
    std::string toDebugString() const {
        if (type == ValueType::STRING) {
            return "\"" + stringValue + "\"";
        }
        return toString();
    }
};

// 类信息结构
class LoongClass {
public:
    std::string name;
    std::string superClass;  // 父类名
    std::map<std::string, LoongValue> methods;
    std::shared_ptr<Environment> closure;
    
    LoongClass(const std::string& n, const std::string& sc);
    
    // 查找方法（包括从父类继承）
    LoongValue findMethod(const std::string& methodName);
};

// 实例信息结构
struct LoongInstance {
    std::shared_ptr<LoongClass> klass;
    std::map<std::string, LoongValue> fields;
};

// 绑定方法（实例+方法）
struct BoundMethod {
    LoongValue instance;
    std::shared_ptr<UserFunction> method;
};

// 运算符重载
inline bool operator==(const LoongValue& a, const LoongValue& b) {
    // 处理 NUMBER 和 BIGINT 之间的比较
    if (a.isNumeric() && b.isNumeric()) {
        if (a.isBigint() && b.isBigint()) {
            return a.bigintValue == b.bigintValue;
        } else if (a.isNumber() && b.isNumber()) {
            return a.numberValue == b.numberValue;
        } else {
            // 一个是 NUMBER，一个是 BIGINT
            if (a.isBigint()) {
                return a.bigintValue == BigInteger(static_cast<long long>(b.numberValue));
            } else {
                return BigInteger(static_cast<long long>(a.numberValue)) == b.bigintValue;
            }
        }
    }
    
    if (a.type != b.type) return false;
    
    switch (a.type) {
        case ValueType::NIL: return true;
        case ValueType::BOOL: return a.boolValue == b.boolValue;
        case ValueType::STRING: return a.stringValue == b.stringValue;
        case ValueType::LIST: return a.listValue == b.listValue;
        case ValueType::DICT: return a.dictValue == b.dictValue;
        default: return false;
    }
}

inline bool operator!=(const LoongValue& a, const LoongValue& b) {
    return !(a == b);
}

inline bool operator<(const LoongValue& a, const LoongValue& b) {
    if (a.type != b.type) return a.type < b.type;
    
    switch (a.type) {
        case ValueType::NUMBER: return a.numberValue < b.numberValue;
        case ValueType::STRING: return a.stringValue < b.stringValue;
        default: return false;
    }
}

inline bool operator<=(const LoongValue& a, const LoongValue& b) {
    return a < b || a == b;
}

inline bool operator>(const LoongValue& a, const LoongValue& b) {
    return !(a <= b);
}

inline bool operator>=(const LoongValue& a, const LoongValue& b) {
    return !(a < b);
}

// 算术运算
inline LoongValue operator+(const LoongValue& a, const LoongValue& b) {
    if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
        return LoongValue::number(a.numberValue + b.numberValue);
    }
    if (a.type == ValueType::STRING || b.type == ValueType::STRING) {
        return LoongValue::string(a.toString() + b.toString());
    }
    return LoongValue::nil();
}

inline LoongValue operator-(const LoongValue& a, const LoongValue& b) {
    if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
        return LoongValue::number(a.numberValue - b.numberValue);
    }
    return LoongValue::nil();
}

inline LoongValue operator*(const LoongValue& a, const LoongValue& b) {
    if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
        return LoongValue::number(a.numberValue * b.numberValue);
    }
    // 字符串重复
    if (a.type == ValueType::STRING && b.type == ValueType::NUMBER) {
        std::string result;
        int times = static_cast<int>(b.numberValue);
        for (int i = 0; i < times; i++) {
            result += a.stringValue;
        }
        return LoongValue::string(result);
    }
    if (a.type == ValueType::NUMBER && b.type == ValueType::STRING) {
        return b * a;
    }
    return LoongValue::nil();
}

inline LoongValue operator/(const LoongValue& a, const LoongValue& b) {
    if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
        if (b.numberValue == 0) {
            throw std::runtime_error("除零错误");
        }
        return LoongValue::number(a.numberValue / b.numberValue);
    }
    return LoongValue::nil();
}

inline LoongValue operator%(const LoongValue& a, const LoongValue& b) {
    if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
        if (b.numberValue == 0) {
            throw std::runtime_error("模零错误");
        }
        return LoongValue::number(static_cast<int>(a.numberValue) % static_cast<int>(b.numberValue));
    }
    return LoongValue::nil();
}

} // namespace loong

#endif // LOONG_VALUE_HPP
