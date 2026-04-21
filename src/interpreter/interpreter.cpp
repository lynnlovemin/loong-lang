#include "interpreter.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "utils/error.hpp"
#include "builtin/http.hpp"
#include "builtin/socket.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <filesystem>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <algorithm>
#include <iomanip>

namespace loong {

// ==================== 大数运算辅助函数 ====================

// 将 LoongValue 转换为 BigInteger
static BigInteger toBigInteger(const LoongValue& v) {
    if (v.isBigint()) {
        return v.bigintValue;
    } else if (v.isNumber()) {
        return BigInteger(static_cast<long long>(v.numberValue));
    } else if (v.isChar()) {
        return BigInteger(static_cast<long long>(static_cast<unsigned char>(v.charValue)));
    }
    return BigInteger(0);
}

// 将 LoongValue 转换为 double（支持 CHAR）
static double toNumber(const LoongValue& v) {
    if (v.isNumber()) {
        return v.numberValue;
    } else if (v.isBigint()) {
        return v.bigintValue.toDouble();
    } else if (v.isChar()) {
        return static_cast<double>(static_cast<unsigned char>(v.charValue));
    }
    return 0;
}

// 将 LoongValue 转换为整数（用于运算）
static LoongValue toNumericValue(const LoongValue& v) {
    if (v.isChar()) {
        return LoongValue::number(static_cast<double>(static_cast<unsigned char>(v.charValue)));
    }
    return v;
}

// 检查是否为数值类型（NUMBER 或 BIGINT）
static bool isNumeric(const LoongValue& v) {
    return v.isNumber() || v.isBigint();
}

// 检查是否为数值或字符类型
static bool isNumericOrChar(const LoongValue& v) {
    return v.isNumber() || v.isBigint() || v.isChar();
}

// 类型名称与值类型匹配检查
static bool checkTypeMatch(const std::string& typeName, const LoongValue& value) {
    if (typeName.empty()) return true;  // 无类型注解，不检查
    if (value.isNil()) return true;     // nil 与任何类型兼容
    
    if (typeName == "number") return value.isNumber();
    if (typeName == "string") return value.isString();
    if (typeName == "bool") return value.isBool();
    if (typeName == "char") return value.isChar();
    if (typeName == "list") return value.isList();
    if (typeName == "dict") return value.isDict();
    if (typeName == "bigint") return value.isBigint();
    if (typeName == "function") return value.isFunction() || value.isBuiltinFunction();
    
    return false;  // 未知类型名
}

// 获取值的类型名称字符串
static std::string getValueTypeName(const LoongValue& value) {
    switch (value.type) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return "bool";
        case ValueType::NUMBER: return "number";
        case ValueType::BIGINT: return "bigint";
        case ValueType::CHAR: return "char";
        case ValueType::STRING: return "string";
        case ValueType::LIST: return "list";
        case ValueType::DICT: return "dict";
        case ValueType::FUNCTION: return "function";
        case ValueType::BUILTIN_FUNCTION: return "function";
        case ValueType::CLASS: return "class";
        case ValueType::INSTANCE: return "instance";
        case ValueType::BOUND_METHOD: return "function";
        default: return "unknown";
    }
}

// 执行加法运算（支持 NUMBER 和 BIGINT）
static LoongValue addValues(const LoongValue& left, const LoongValue& right) {
    // 如果两边都是普通数字且结果不会溢出，使用 double
    if (left.isNumber() && right.isNumber()) {
        double result = left.numberValue + right.numberValue;
        // 检查是否超出安全整数范围
        if (result >= -9007199254740991.0 && result <= 9007199254740991.0) {
            return LoongValue::number(result);
        }
        // 转换为 BigInteger
        return LoongValue::bigint(toBigInteger(left) + toBigInteger(right));
    }
    // 至少有一个是 BIGINT 或者结果溢出，使用 BigInteger
    return LoongValue::bigint(toBigInteger(left) + toBigInteger(right));
}

// 执行减法运算
static LoongValue subtractValues(const LoongValue& left, const LoongValue& right) {
    if (left.isNumber() && right.isNumber()) {
        double result = left.numberValue - right.numberValue;
        if (result >= -9007199254740991.0 && result <= 9007199254740991.0) {
            return LoongValue::number(result);
        }
        return LoongValue::bigint(toBigInteger(left) - toBigInteger(right));
    }
    return LoongValue::bigint(toBigInteger(left) - toBigInteger(right));
}

// 执行乘法运算
static LoongValue multiplyValues(const LoongValue& left, const LoongValue& right) {
    // 如果任一操作数是 BIGINT，使用 BigInteger 运算
    if (left.isBigint() || right.isBigint()) {
        return LoongValue::bigint(toBigInteger(left) * toBigInteger(right));
    }
    
    // 两个都是 NUMBER，检查是否会溢出
    double a = left.numberValue;
    double b = right.numberValue;
    
    // 检查是否会溢出
    // 使用对数来预测结果的位数
    if (a != 0 && b != 0) {
        double logResult = std::log10(std::abs(a)) + std::log10(std::abs(b));
        // 如果结果可能超过15位有效数字，使用 BigInteger
        if (logResult > 14) {
            return LoongValue::bigint(toBigInteger(left) * toBigInteger(right));
        }
    }
    
    double result = a * b;
    
    // 检查结果是否为整数且在安全范围内
    if (result == std::floor(result) && 
        result >= -9007199254740991.0 && result <= 9007199254740991.0) {
        return LoongValue::number(result);
    }
    
    // 如果结果超出安全整数范围，转换为 BigInteger
    if (result == std::floor(result)) {
        return LoongValue::bigint(toBigInteger(left) * toBigInteger(right));
    }
    
    // 非整数结果，返回 double
    return LoongValue::number(result);
}

// 执行除法运算（总是返回 NUMBER，因为可能是小数）
static LoongValue divideValues(const LoongValue& left, const LoongValue& right) {
    if (right.isBigint() && right.bigintValue.isZero()) {
        throw std::runtime_error("除零错误");
    }
    if (right.isNumber() && right.numberValue == 0) {
        throw std::runtime_error("除零错误");
    }
    
    // 除法可能产生小数，使用浮点数
    double leftVal = left.isBigint() ? left.bigintValue.toDouble() : left.numberValue;
    double rightVal = right.isBigint() ? right.bigintValue.toDouble() : right.numberValue;
    return LoongValue::number(leftVal / rightVal);
}

// 执行取模运算
static LoongValue moduloValues(const LoongValue& left, const LoongValue& right) {
    if ((right.isBigint() && right.bigintValue.isZero()) ||
        (right.isNumber() && right.numberValue == 0)) {
        throw std::runtime_error("模零错误");
    }
    // 对于普通数字和字符，使用简单的整数取模
    if (!left.isBigint() && !right.isBigint()) {
        long long leftVal = static_cast<long long>(toNumber(left));
        long long rightVal = static_cast<long long>(toNumber(right));
        return LoongValue::number(static_cast<double>(leftVal % rightVal));
    }
    return LoongValue::bigint(toBigInteger(left) % toBigInteger(right));
}

// 比较两个值的大小
static bool compareLess(const LoongValue& left, const LoongValue& right) {
    if (left.isNumber() && right.isNumber()) {
        return left.numberValue < right.numberValue;
    }
    return toBigInteger(left) < toBigInteger(right);
}

static bool compareGreater(const LoongValue& left, const LoongValue& right) {
    if (left.isNumber() && right.isNumber()) {
        return left.numberValue > right.numberValue;
    }
    return toBigInteger(left) > toBigInteger(right);
}

static bool compareLessEqual(const LoongValue& left, const LoongValue& right) {
    if (left.isNumber() && right.isNumber()) {
        return left.numberValue <= right.numberValue;
    }
    return toBigInteger(left) <= toBigInteger(right);
}

static bool compareGreaterEqual(const LoongValue& left, const LoongValue& right) {
    if (left.isNumber() && right.isNumber()) {
        return left.numberValue >= right.numberValue;
    }
    return toBigInteger(left) >= toBigInteger(right);
}

// ==================== Interpreter 实现 ====================

Interpreter::Interpreter() {
    globals_ = std::make_shared<Environment>();
    environment_ = globals_;
    registerBuiltinFunctions();
}

// ==================== 表达式求值 ====================

LoongValue Interpreter::evaluate(ExprPtr expr) {
    if (!expr) return LoongValue::nil();
    
    if (auto e = dynamic_cast<LiteralExpr*>(expr.get())) {
        return visitLiteralExpr(e);
    }
    if (auto e = dynamic_cast<IdentifierExpr*>(expr.get())) {
        return visitIdentifierExpr(e);
    }
    if (auto e = dynamic_cast<BinaryExpr*>(expr.get())) {
        return visitBinaryExpr(e);
    }
    if (auto e = dynamic_cast<UnaryExpr*>(expr.get())) {
        return visitUnaryExpr(e);
    }
    if (auto e = dynamic_cast<GroupedExpr*>(expr.get())) {
        return visitGroupedExpr(e);
    }
    if (auto e = dynamic_cast<ListExpr*>(expr.get())) {
        return visitListExpr(e);
    }
    if (auto e = dynamic_cast<DictExpr*>(expr.get())) {
        return visitDictExpr(e);
    }
    if (auto e = dynamic_cast<IndexExpr*>(expr.get())) {
        return visitIndexExpr(e);
    }
    if (auto e = dynamic_cast<MemberExpr*>(expr.get())) {
        return visitMemberExpr(e);
    }
    if (auto e = dynamic_cast<CallExpr*>(expr.get())) {
        return visitCallExpr(e);
    }
    if (auto e = dynamic_cast<AssignExpr*>(expr.get())) {
        return visitAssignExpr(e);
    }
    if (auto e = dynamic_cast<ThisExpr*>(expr.get())) {
        return visitThisExpr(e);
    }
    if (auto e = dynamic_cast<SuperExpr*>(expr.get())) {
        return visitSuperExpr(e);
    }
    if (auto e = dynamic_cast<LambdaExpr*>(expr.get())) {
        return visitLambdaExpr(e);
    }
    if (auto e = dynamic_cast<TernaryExpr*>(expr.get())) {
        return visitTernaryExpr(e);
    }
    if (auto e = dynamic_cast<ChannelSendExpr*>(expr.get())) {
        return visitChannelSendExpr(e);
    }
    if (auto e = dynamic_cast<ChannelRecvExpr*>(expr.get())) {
        return visitChannelRecvExpr(e);
    }
    if (auto e = dynamic_cast<MutexLockExpr*>(expr.get())) {
        return visitMutexLockExpr(e);
    }
    if (auto e = dynamic_cast<MutexUnlockExpr*>(expr.get())) {
        return visitMutexUnlockExpr(e);
    }

    return LoongValue::nil();
}

LoongValue Interpreter::visitLiteralExpr(LiteralExpr* expr) {
    switch (expr->literalType) {
        case LiteralExpr::LiteralType::NIL:
            return LoongValue::nil();
        case LiteralExpr::LiteralType::BOOL:
            return LoongValue::boolean(expr->boolValue);
        case LiteralExpr::LiteralType::NUMBER:
            return LoongValue::number(expr->numberValue);
        case LiteralExpr::LiteralType::BIGINT:
            return LoongValue::bigint(expr->bigintString);
        case LiteralExpr::LiteralType::CHAR:
            return LoongValue::charVal(expr->charValue);
        case LiteralExpr::LiteralType::STRING:
            return LoongValue::string(expr->stringValue);
        default:
            return LoongValue::nil();
    }
}

LoongValue Interpreter::visitIdentifierExpr(IdentifierExpr* expr) {
    try {
        return environment_->get(expr->name);
    } catch (const std::runtime_error& e) {
        LOONG_NAME_ERROR("未定义的变量: " + expr->name);
    }
    return LoongValue::nil();
}

LoongValue Interpreter::visitBinaryExpr(BinaryExpr* expr) {
    LoongValue left = evaluate(expr->left);
    LoongValue right = evaluate(expr->right);
    
    // 如果任一操作数是字符，先转换为数值进行运算
    LoongValue leftNum = toNumericValue(left);
    LoongValue rightNum = toNumericValue(right);
    
    switch (expr->op.type) {
        case TokenType::PLUS:
            if (isNumericOrChar(left) && isNumericOrChar(right)) {
                return addValues(leftNum, rightNum);
            }
            if (left.isString() || right.isString()) {
                return LoongValue::string(left.toString() + right.toString());
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行加法运算");
            break;
            
        case TokenType::MINUS:
            if (isNumericOrChar(left) && isNumericOrChar(right)) {
                return subtractValues(leftNum, rightNum);
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行减法运算");
            break;
            
        case TokenType::STAR:
            if (isNumericOrChar(left) && isNumericOrChar(right)) {
                return multiplyValues(leftNum, rightNum);
            }
            if (left.isString() && isNumericOrChar(right)) {
                return left * rightNum;
            }
            if (isNumericOrChar(left) && right.isString()) {
                return right * leftNum;
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行乘法运算");
            break;
            
        case TokenType::SLASH:
            if (isNumericOrChar(left) && isNumericOrChar(right)) {
                try {
                    return divideValues(leftNum, rightNum);
                } catch (const std::runtime_error& e) {
                    LOONG_RUNTIME_ERROR(e.what());
                }
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行除法运算");
            break;
            
        case TokenType::PERCENT:
            if (isNumericOrChar(left) && isNumericOrChar(right)) {
                try {
                    return moduloValues(leftNum, rightNum);
                } catch (const std::runtime_error& e) {
                    LOONG_RUNTIME_ERROR(e.what());
                }
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行取模运算");
            break;
            
        case TokenType::EQUAL_EQUAL:
            return LoongValue::boolean(left == right);
            
        case TokenType::BANG_EQUAL:
            return LoongValue::boolean(left != right);
            
        case TokenType::LESS:
            if (isNumericOrChar(left) && isNumericOrChar(right)) {
                return LoongValue::boolean(compareLess(leftNum, rightNum));
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行比较");
            break;
            
        case TokenType::LESS_EQUAL:
            if (isNumericOrChar(left) && isNumericOrChar(right)) {
                return LoongValue::boolean(compareLessEqual(leftNum, rightNum));
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行比较");
            break;
            
        case TokenType::GREATER:
            if (isNumericOrChar(left) && isNumericOrChar(right)) {
                return LoongValue::boolean(compareGreater(leftNum, rightNum));
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行比较");
            break;
            
        case TokenType::GREATER_EQUAL:
            if (isNumericOrChar(left) && isNumericOrChar(right)) {
                return LoongValue::boolean(compareGreaterEqual(leftNum, rightNum));
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行比较");
            break;
            
        case TokenType::AND:
        case TokenType::AND_AND:  // &&
            return LoongValue::boolean(left.isTruthy() && right.isTruthy());
            
        case TokenType::OR:
        case TokenType::OR_OR:  // ||
            return LoongValue::boolean(left.isTruthy() || right.isTruthy());
        
        // 位运算符
        case TokenType::BITAND:  // &
            if (left.isNumber() && right.isNumber()) {
                return LoongValue::number((double)((long long)left.numberValue & (long long)right.numberValue));
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行位与运算");
            break;
            
        case TokenType::BITOR:  // |
            if (left.isNumber() && right.isNumber()) {
                return LoongValue::number((double)((long long)left.numberValue | (long long)right.numberValue));
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行位或运算");
            break;
            
        case TokenType::XOR:  // ^
            if (left.isNumber() && right.isNumber()) {
                return LoongValue::number((double)((long long)left.numberValue ^ (long long)right.numberValue));
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行异或运算");
            break;
            
        case TokenType::LSHIFT:  // <<
            if (left.isNumber() && right.isNumber()) {
                return LoongValue::number((double)((long long)left.numberValue << (int)right.numberValue));
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行左移运算");
            break;
            
        case TokenType::RSHIFT:  // >>
            if (left.isNumber() && right.isNumber()) {
                return LoongValue::number((double)((long long)left.numberValue >> (int)right.numberValue));
            }
            LOONG_TYPE_ERROR("无法对 " + left.typeName() + " 和 " + right.typeName() + " 进行右移运算");
            break;
            
        default:
            break;
    }
    
    return LoongValue::nil();
}

LoongValue Interpreter::visitUnaryExpr(UnaryExpr* expr) {
    LoongValue right = evaluate(expr->operand);
    
    switch (expr->op.type) {
        case TokenType::MINUS:
            if (right.isNumber()) {
                return LoongValue::number(-right.numberValue);
            }
            LOONG_TYPE_ERROR("无法对 " + right.typeName() + " 取负");
            break;
            
        case TokenType::NOT:
        case TokenType::BANG:  // !
            return LoongValue::boolean(!right.isTruthy());
            
        case TokenType::BITNOT:  // ~
            if (right.isNumber()) {
                return LoongValue::number((double)(~(long long)right.numberValue));
            }
            LOONG_TYPE_ERROR("无法对 " + right.typeName() + " 进行位取反运算");
            break;
            
        default:
            break;
    }
    
    return LoongValue::nil();
}

LoongValue Interpreter::visitGroupedExpr(GroupedExpr* expr) {
    return evaluate(expr->expression);
}

LoongValue Interpreter::visitListExpr(ListExpr* expr) {
    std::vector<LoongValue> elements;
    for (const auto& e : expr->elements) {
        elements.push_back(evaluate(e));
    }
    return LoongValue::list(elements);
}

LoongValue Interpreter::visitDictExpr(DictExpr* expr) {
    std::map<std::string, LoongValue> dict;
    for (const auto& pair : expr->pairs) {
        LoongValue key = evaluate(pair.first);
        LoongValue value = evaluate(pair.second);
        if (!key.isString()) {
            LOONG_TYPE_ERROR("字典键必须是字符串类型");
        }
        dict[key.stringValue] = value;
    }
    return LoongValue::dict(dict);
}

LoongValue Interpreter::visitIndexExpr(IndexExpr* expr) {
    LoongValue object = evaluate(expr->object);
    LoongValue index = evaluate(expr->index);
    
    if (object.isList()) {
        if (!index.isNumber()) {
            LOONG_TYPE_ERROR("列表索引必须是数字");
        }
        int idx = static_cast<int>(index.numberValue);
        if (idx < 0 || idx >= static_cast<int>(object.listValue.size())) {
            LOONG_INDEX_ERROR("列表索引越界: " + std::to_string(idx));
        }
        return object.listValue[idx];
    }
    
    if (object.isDict()) {
        if (!index.isString()) {
            LOONG_TYPE_ERROR("字典键必须是字符串");
        }
        auto it = object.dictValue.find(index.stringValue);
        if (it == object.dictValue.end()) {
            return LoongValue::nil();
        }
        return it->second;
    }
    
    if (object.isString()) {
        if (!index.isNumber()) {
            LOONG_TYPE_ERROR("字符串索引必须是数字");
        }
        int idx = static_cast<int>(index.numberValue);
        if (idx < 0 || idx >= static_cast<int>(object.stringValue.size())) {
            LOONG_INDEX_ERROR("字符串索引越界: " + std::to_string(idx));
        }
        return LoongValue::string(std::string(1, object.stringValue[idx]));
    }
    
    LOONG_TYPE_ERROR("无法对 " + object.typeName() + " 类型进行索引访问");
    return LoongValue::nil();
}

LoongValue Interpreter::visitMemberExpr(MemberExpr* expr) {
    LoongValue object = evaluate(expr->object);
    
    // 字典成员访问
    if (object.isDict()) {
        auto it = object.dictValue.find(expr->member);
        if (it == object.dictValue.end()) {
            return LoongValue::nil();
        }
        return it->second;
    }
    
    // 内置方法
    if (object.isString()) {
        if (expr->member == "len") {
            return LoongValue::number(object.stringValue.length());
        }
        if (expr->member == "upper") {
            std::string upper = object.stringValue;
            for (char& c : upper) c = std::toupper(c);
            return LoongValue::string(upper);
        }
        if (expr->member == "lower") {
            std::string lower = object.stringValue;
            for (char& c : lower) c = std::tolower(c);
            return LoongValue::string(lower);
        }
    }
    
    if (object.isList()) {
        if (expr->member == "len") {
            return LoongValue::number(object.listValue.size());
        }
    }
    
    // 实例成员访问
    if (object.isInstance()) {
        auto instance = object.instanceValue;
        auto klass = instance->klass;
        
        // 1. 先查找实例属性
        auto fieldIt = instance->fields.find(expr->member);
        if (fieldIt != instance->fields.end()) {
            return fieldIt->second;
        }
        
        // 2. 查找方法
        auto methodIt = klass->methods.find(expr->member);
        if (methodIt != klass->methods.end()) {
            // 返回绑定方法（包含实例引用）
            auto bm = std::make_shared<BoundMethod>();
            bm->instance = object;
            bm->method = methodIt->second.userFunc;
            return LoongValue::boundMethod(bm);
        }
        
        // 3. 查找父类方法
        if (!klass->superClass.empty()) {
            try {
                LoongValue superVal = environment_->get(klass->superClass);
                if (superVal.isClass()) {
                    auto superMethodIt = superVal.classValue->methods.find(expr->member);
                    if (superMethodIt != superVal.classValue->methods.end()) {
                        return superMethodIt->second;
                    }
                }
            } catch (...) {
                // 父类未找到
            }
        }
        
        return LoongValue::nil();
    }

    LOONG_TYPE_ERROR(object.typeName() + " 类型没有成员: " + expr->member);
    return LoongValue::nil();
}

LoongValue Interpreter::visitCallExpr(CallExpr* expr) {
    // 处理列表/字典/字符串方法调用
    if (auto memberExpr = dynamic_cast<MemberExpr*>(expr->callee.get())) {
        LoongValue object = evaluate(memberExpr->object);
        std::vector<LoongValue> arguments;
        for (const auto& arg : expr->arguments) {
            arguments.push_back(evaluate(arg));
        }
        
        // 列表方法
        if (object.isList()) {
            if (memberExpr->member == "push") {
                for (const auto& arg : arguments) {
                    object.listValue.push_back(arg);
                }
                // 更新原变量
                if (auto ident = dynamic_cast<IdentifierExpr*>(memberExpr->object.get())) {
                    if (environment_->exists(ident->name)) {
                        environment_->assign(ident->name, object);
                    }
                }
                return LoongValue::nil();
            }
            if (memberExpr->member == "pop") {
                if (object.listValue.empty()) {
                    return LoongValue::nil();
                }
                auto last = object.listValue.back();
                object.listValue.pop_back();
                if (auto ident = dynamic_cast<IdentifierExpr*>(memberExpr->object.get())) {
                    if (environment_->exists(ident->name)) {
                        environment_->assign(ident->name, object);
                    }
                }
                return last;
            }
            if (memberExpr->member == "insert") {
                if (arguments.size() >= 2 && arguments[0].isNumber()) {
                    int idx = static_cast<int>(arguments[0].numberValue);
                    if (idx < 0) idx = 0;
                    if (idx > static_cast<int>(object.listValue.size())) idx = static_cast<int>(object.listValue.size());
                    object.listValue.insert(object.listValue.begin() + idx, arguments[1]);
                    if (auto ident = dynamic_cast<IdentifierExpr*>(memberExpr->object.get())) {
                        if (environment_->exists(ident->name)) {
                            environment_->assign(ident->name, object);
                        }
                    }
                }
                return LoongValue::nil();
            }
            if (memberExpr->member == "remove") {
                if (!arguments.empty() && arguments[0].isNumber()) {
                    int idx = static_cast<int>(arguments[0].numberValue);
                    if (idx >= 0 && idx < static_cast<int>(object.listValue.size())) {
                        object.listValue.erase(object.listValue.begin() + idx);
                        if (auto ident = dynamic_cast<IdentifierExpr*>(memberExpr->object.get())) {
                            if (environment_->exists(ident->name)) {
                                environment_->assign(ident->name, object);
                            }
                        }
                    }
                }
                return LoongValue::nil();
            }
            if (memberExpr->member == "join") {
                std::string sep = arguments.empty() ? "" : arguments[0].stringValue;
                std::string result;
                for (size_t i = 0; i < object.listValue.size(); i++) {
                    if (i > 0) result += sep;
                    result += object.listValue[i].toString();
                }
                return LoongValue::string(result);
            }
        }
        
        // 字典方法
        if (object.isDict()) {
            if (memberExpr->member == "keys") {
                std::vector<LoongValue> keys;
                for (const auto& kv : object.dictValue) {
                    keys.push_back(LoongValue::string(kv.first));
                }
                return LoongValue::list(keys);
            }
            if (memberExpr->member == "values") {
                std::vector<LoongValue> values;
                for (const auto& kv : object.dictValue) {
                    values.push_back(kv.second);
                }
                return LoongValue::list(values);
            }
            if (memberExpr->member == "has") {
                if (!arguments.empty() && arguments[0].isString()) {
                    return LoongValue::number(object.dictValue.count(arguments[0].stringValue) > 0 ? 1 : 0);
                }
                return LoongValue::number(0);
            }
            if (memberExpr->member == "remove") {
                if (!arguments.empty() && arguments[0].isString()) {
                    object.dictValue.erase(arguments[0].stringValue);
                    if (auto ident = dynamic_cast<IdentifierExpr*>(memberExpr->object.get())) {
                        if (environment_->exists(ident->name)) {
                            environment_->assign(ident->name, object);
                        }
                    }
                }
                return LoongValue::nil();
            }
        }
        
        // 字符串方法
        if (object.isString()) {
            if (memberExpr->member == "contains") {
                if (!arguments.empty() && arguments[0].isString()) {
                    return LoongValue::number(object.stringValue.find(arguments[0].stringValue) != std::string::npos ? 1 : 0);
                }
                return LoongValue::number(0);
            }
            if (memberExpr->member == "startsWith") {
                if (!arguments.empty() && arguments[0].isString()) {
                    return LoongValue::number(object.stringValue.find(arguments[0].stringValue) == 0 ? 1 : 0);
                }
                return LoongValue::number(0);
            }
            if (memberExpr->member == "endsWith") {
                if (!arguments.empty() && arguments[0].isString()) {
                    const std::string& suffix = arguments[0].stringValue;
                    if (suffix.size() > object.stringValue.size()) return LoongValue::number(0);
                    return LoongValue::number(object.stringValue.compare(object.stringValue.size() - suffix.size(), suffix.size(), suffix) == 0 ? 1 : 0);
                }
                return LoongValue::number(0);
            }
            if (memberExpr->member == "replace") {
                if (arguments.size() >= 2 && arguments[0].isString() && arguments[1].isString()) {
                    std::string result = object.stringValue;
                    size_t pos = 0;
                    while ((pos = result.find(arguments[0].stringValue, pos)) != std::string::npos) {
                        result.replace(pos, arguments[0].stringValue.size(), arguments[1].stringValue);
                        pos += arguments[1].stringValue.size();
                    }
                    return LoongValue::string(result);
                }
                return LoongValue::string(object.stringValue);
            }
            if (memberExpr->member == "split") {
                std::string sep = arguments.empty() ? " " : arguments[0].stringValue;
                std::vector<LoongValue> parts;
                if (sep.empty()) {
                    for (char c : object.stringValue) {
                        parts.push_back(LoongValue::string(std::string(1, c)));
                    }
                } else {
                    size_t start = 0;
                    size_t pos = object.stringValue.find(sep);
                    while (pos != std::string::npos) {
                        parts.push_back(LoongValue::string(object.stringValue.substr(start, pos - start)));
                        start = pos + sep.size();
                        pos = object.stringValue.find(sep, start);
                    }
                    parts.push_back(LoongValue::string(object.stringValue.substr(start)));
                }
                return LoongValue::list(parts);
            }
            if (memberExpr->member == "trim") {
                std::string s = object.stringValue;
                s.erase(0, s.find_first_not_of(" \t\n\r"));
                s.erase(s.find_last_not_of(" \t\n\r") + 1);
                return LoongValue::string(s);
            }
        }
    }
    
    LoongValue callee = evaluate(expr->callee);
    
    // 类构造函数调用
    if (callee.isClass()) {
        auto klass = callee.classValue;
        
        // 创建实例
        auto instance = std::make_shared<LoongInstance>();
        instance->klass = klass;
        
        std::vector<LoongValue> arguments;
        for (const auto& arg : expr->arguments) {
            arguments.push_back(evaluate(arg));
        }
        
        // 查找构造函数
        auto initIt = klass->methods.find("__init__");
        if (initIt != klass->methods.end()) {
            // 执行构造函数
            auto initFunc = initIt->second.userFunc;
            
            if (arguments.size() < initFunc->params.size()) {
                LOONG_RUNTIME_ERROR("构造函数需要至少 " + 
                                  std::to_string(initFunc->params.size()) + " 个参数");
            }
            
            // 保存当前实例和类
            auto previousInstance = currentInstance_;
            auto previousClass = currentClass_;
            auto previousSuperClass = currentSuperClass_;
            
            currentInstance_ = LoongValue::instance(instance);
            currentClass_ = klass;
            
            // 设置父类
            if (!klass->superClass.empty()) {
                try {
                    LoongValue superVal = environment_->get(klass->superClass);
                    if (superVal.isClass()) {
                        currentSuperClass_ = superVal.classValue;
                    }
                } catch (...) {
                    // 父类未找到
                }
            }
            
            // 创建新环境
            auto previous = environment_;
            auto newEnv = initFunc->closure->createChild();
            environment_ = newEnv;
            
            // 绑定参数
            for (size_t i = 0; i < initFunc->params.size(); i++) {
                environment_->define(initFunc->params[i], arguments[i]);
            }
            
            // 执行构造函数体
            try {
                for (const auto& stmt : initFunc->body) {
                    execute(stmt);
                }
            } catch (const ReturnException&) {
                // 构造函数的return被忽略
            }
            
            environment_ = previous;
            currentInstance_ = previousInstance;
            currentClass_ = previousClass;
            currentSuperClass_ = previousSuperClass;
        }
        
        return LoongValue::instance(instance);
    }
        
    // 绑定方法调用
    if (callee.isBoundMethod()) {
        auto bm = callee.boundMethodValue;
        auto func = bm->method;
            
        std::vector<LoongValue> arguments;
        for (const auto& arg : expr->arguments) {
            arguments.push_back(evaluate(arg));
        }
            
        if (arguments.size() < func->params.size()) {
            LOONG_RUNTIME_ERROR("方法 " + func->name + " 需要至少 " + 
                              std::to_string(func->params.size()) + " 个参数");
        }
            
        // 保存当前实例和类
        auto previousInstance = currentInstance_;
        auto previousClass = currentClass_;
        auto previousSuperClass = currentSuperClass_;
            
        currentInstance_ = bm->instance;
        if (bm->instance.isInstance()) {
            currentClass_ = bm->instance.instanceValue->klass;
            // 设置父类
            if (!currentClass_->superClass.empty()) {
                try {
                    LoongValue superVal = environment_->get(currentClass_->superClass);
                    if (superVal.isClass()) {
                        currentSuperClass_ = superVal.classValue;
                    }
                } catch (...) {
                    // 父类未找到
                }
            }
        }
            
        // 创建新环境
        auto previous = environment_;
        auto newEnv = func->closure->createChild();
        environment_ = newEnv;
            
        // 绑定参数
        for (size_t i = 0; i < func->params.size(); i++) {
            environment_->define(func->params[i], arguments[i]);
        }
            
        // 绑定默认参数
        for (size_t i = 0; i < func->defaultParams.size(); i++) {
            size_t argIndex = func->params.size() + i;
            if (argIndex < arguments.size()) {
                environment_->define(func->defaultParams[i].name, arguments[argIndex]);
            } else {
                environment_->define(func->defaultParams[i].name, evaluate(func->defaultParams[i].defaultValue));
            }
        }
            
        // 执行方法体
        LoongValue result = LoongValue::nil();
        try {
            for (const auto& stmt : func->body) {
                execute(stmt);
            }
        } catch (const ReturnException& ret) {
            result = ret.value;
        }
            
        environment_ = previous;
        currentInstance_ = previousInstance;
        currentClass_ = previousClass;
        currentSuperClass_ = previousSuperClass;
            
        return result;
    }
    
    if (!callee.isFunction()) {
        LOONG_TYPE_ERROR("只能调用函数");
    }
    
    std::vector<LoongValue> arguments;
    for (const auto& arg : expr->arguments) {
        arguments.push_back(evaluate(arg));
    }
    
    // 内置函数
    if (callee.isBuiltinFunction()) {
        return callee.builtinFunc(arguments);
    }
    
    // 用户定义函数
    if (callee.isUserFunction()) {
        auto func = callee.userFunc;
        
        if (arguments.size() < func->params.size()) {
            LOONG_RUNTIME_ERROR("函数 " + func->name + " 需要至少 " + 
                              std::to_string(func->params.size()) + " 个参数，但只提供了 " +
                              std::to_string(arguments.size()) + " 个");
        }
        
        if (arguments.size() > func->params.size() + func->defaultParams.size()) {
            LOONG_RUNTIME_ERROR("函数 " + func->name + " 最多接受 " + 
                              std::to_string(func->params.size() + func->defaultParams.size()) + " 个参数");
        }
        
        // 创建新的环境
        auto previous = environment_;
        auto newEnv = func->closure->createChild();
        environment_ = newEnv;
        
        // 绑定参数
        for (size_t i = 0; i < func->params.size(); i++) {
            environment_->define(func->params[i], arguments[i]);
        }
        
        // 绑定默认参数
        for (size_t i = 0; i < func->defaultParams.size(); i++) {
            size_t argIndex = func->params.size() + i;
            if (argIndex < arguments.size()) {
                environment_->define(func->defaultParams[i].name, arguments[argIndex]);
            } else {
                environment_->define(func->defaultParams[i].name, evaluate(func->defaultParams[i].defaultValue));
            }
        }
        
        // 执行函数体
        LoongValue result = LoongValue::nil();
        try {
            for (const auto& stmt : func->body) {
                execute(stmt);
            }
        } catch (const ReturnException& ret) {
            result = ret.value;
        }
        
        environment_ = previous;
        return result;
    }
    
    return LoongValue::nil();
}

LoongValue Interpreter::visitAssignExpr(AssignExpr* expr) {
    LoongValue value = evaluate(expr->value);
    
    // 赋值给变量
    if (auto ident = dynamic_cast<IdentifierExpr*>(expr->target.get())) {
        if (!environment_->exists(ident->name)) {
            LOONG_NAME_ERROR("未定义的变量: " + ident->name);
        }
        if (environment_->isConst(ident->name)) {
            LOONG_RUNTIME_ERROR("无法修改常量: " + ident->name);
        }
        // 类型检查：如果变量有类型注解，验证赋值类型匹配
        std::string varType = environment_->getType(ident->name);
        if (!varType.empty() && !checkTypeMatch(varType, value)) {
            LOONG_TYPE_ERROR("类型不匹配: 变量 '" + ident->name + "' 声明为 " + varType + " 类型，但赋值为 " + getValueTypeName(value) + " 类型");
        }
        environment_->assign(ident->name, value);
        return value;
    }
    
    // 赋值给索引
    if (auto indexExpr = dynamic_cast<IndexExpr*>(expr->target.get())) {
        LoongValue object = evaluate(indexExpr->object);
        LoongValue index = evaluate(indexExpr->index);
        
        if (object.isList()) {
            if (!index.isNumber()) {
                LOONG_TYPE_ERROR("列表索引必须是数字");
            }
            int idx = static_cast<int>(index.numberValue);
            if (idx < 0 || idx >= static_cast<int>(object.listValue.size())) {
                LOONG_INDEX_ERROR("列表索引越界");
            }
            // 需要修改原列表
            environment_->assign(dynamic_cast<IdentifierExpr*>(indexExpr->object.get())->name, 
                               LoongValue::list(object.listValue));
            return value;
        }
        
        if (object.isDict()) {
            if (!index.isString()) {
                LOONG_TYPE_ERROR("字典键必须是字符串");
            }
            object.dictValue[index.stringValue] = value;
            return value;
        }
        
        LOONG_TYPE_ERROR("无法对 " + object.typeName() + " 进行索引赋值");
    }
    
    // 赋值给成员（实例属性）
    if (auto memberExpr = dynamic_cast<MemberExpr*>(expr->target.get())) {
        LoongValue object = evaluate(memberExpr->object);
        
        if (object.isInstance()) {
            object.instanceValue->fields[memberExpr->member] = value;
            return value;
        }
        
        LOONG_TYPE_ERROR("无法对 " + object.typeName() + " 进行成员赋值");
    }
    
    LOONG_RUNTIME_ERROR("无效的赋值目标");
    return LoongValue::nil();
}

// ==================== 语句执行 ====================

void Interpreter::execute(StmtPtr stmt) {
    if (!stmt) return;
    
    if (auto s = dynamic_cast<ExprStmt*>(stmt.get())) {
        visitExprStmt(s);
    } else if (auto s = dynamic_cast<ValStmt*>(stmt.get())) {
        visitValStmt(s);
    } else if (auto s = dynamic_cast<ConstStmt*>(stmt.get())) {
        visitConstStmt(s);
    } else if (auto s = dynamic_cast<FnStmt*>(stmt.get())) {
        visitFnStmt(s);
    } else if (auto s = dynamic_cast<ReturnStmt*>(stmt.get())) {
        visitReturnStmt(s);
    } else if (auto s = dynamic_cast<IfStmt*>(stmt.get())) {
        visitIfStmt(s);
    } else if (auto s = dynamic_cast<WhileStmt*>(stmt.get())) {
        visitWhileStmt(s);
    } else if (auto s = dynamic_cast<ForStmt*>(stmt.get())) {
        visitForStmt(s);
    } else if (auto s = dynamic_cast<ForInStmt*>(stmt.get())) {
        visitForInStmt(s);
    } else if (auto s = dynamic_cast<BreakStmt*>(stmt.get())) {
        visitBreakStmt(s);
    } else if (auto s = dynamic_cast<ContinueStmt*>(stmt.get())) {
        visitContinueStmt(s);
    } else if (auto s = dynamic_cast<ImportStmt*>(stmt.get())) {
        visitImportStmt(s);
    } else if (auto s = dynamic_cast<TryStmt*>(stmt.get())) {
        visitTryStmt(s);
    } else if (auto s = dynamic_cast<ThrowStmt*>(stmt.get())) {
        visitThrowStmt(s);
    } else if (auto s = dynamic_cast<ClassStmt*>(stmt.get())) {
        visitClassStmt(s);
    } else if (auto s = dynamic_cast<SwitchStmt*>(stmt.get())) {
        visitSwitchStmt(s);
    } else if (auto s = dynamic_cast<SpawnStmt*>(stmt.get())) {
        visitSpawnStmt(s);
    } else if (auto s = dynamic_cast<LockStmt*>(stmt.get())) {
        visitLockStmt(s);
    }
}

void Interpreter::executeBlock(const std::vector<StmtPtr>& statements,
                               std::shared_ptr<Environment> environment) {
    auto previous = environment_;
    environment_ = environment;
    
    try {
        for (const auto& stmt : statements) {
            execute(stmt);
        }
    } catch (...) {
        environment_ = previous;
        throw;
    }
    
    environment_ = previous;
}

void Interpreter::visitExprStmt(ExprStmt* stmt) {
    evaluate(stmt->expression);
}

void Interpreter::visitValStmt(ValStmt* stmt) {
    LoongValue value = LoongValue::nil();
    if (stmt->initializer) {
        value = evaluate(stmt->initializer);
    }
    
    // 类型检查：如果有类型注解且有初始化值，验证类型匹配
    if (!stmt->typeName.empty() && stmt->initializer) {
        if (!checkTypeMatch(stmt->typeName, value)) {
            LOONG_TYPE_ERROR("类型不匹配: 变量 '" + stmt->name + "' 声明为 " + stmt->typeName + " 类型，但初始化值为 " + getValueTypeName(value) + " 类型");
        }
    }
    
    environment_->defineWithType(stmt->name, value, stmt->typeName);
}

void Interpreter::visitConstStmt(ConstStmt* stmt) {
    LoongValue value = LoongValue::nil();
    if (stmt->initializer) {
        value = evaluate(stmt->initializer);
    }
    
    // 类型检查：如果有类型注解，验证类型匹配
    if (!stmt->typeName.empty()) {
        if (!checkTypeMatch(stmt->typeName, value)) {
            LOONG_TYPE_ERROR("类型不匹配: 常量 '" + stmt->name + "' 声明为 " + stmt->typeName + " 类型，但初始化值为 " + getValueTypeName(value) + " 类型");
        }
    }
    
    environment_->defineConstWithType(stmt->name, value, stmt->typeName);
}

void Interpreter::visitFnStmt(FnStmt* stmt) {
    auto func = std::make_shared<UserFunction>();
    func->name = stmt->name;
    func->params = stmt->params;
    
    // 转换默认参数
    for (const auto& dp : stmt->defaultParams) {
        func->defaultParams.push_back(DefaultParamInfo(dp.first, dp.second));
    }
    
    func->closure = environment_;
    func->body = stmt->body;
    
    environment_->define(stmt->name, LoongValue::userFunction(func));
}

void Interpreter::visitReturnStmt(ReturnStmt* stmt) {
    LoongValue value = LoongValue::nil();
    if (stmt->value) {
        value = evaluate(stmt->value);
    }
    throw ReturnException(value);
}

void Interpreter::visitIfStmt(IfStmt* stmt) {
    if (evaluate(stmt->condition).isTruthy()) {
        executeBlock(stmt->thenBranch, environment_->createChild());
    } else if (!stmt->elseBranch.empty()) {
        executeBlock(stmt->elseBranch, environment_->createChild());
    }
}

void Interpreter::visitWhileStmt(WhileStmt* stmt) {
    while (evaluate(stmt->condition).isTruthy()) {
        try {
            executeBlock(stmt->body, environment_->createChild());
        } catch (const BreakException&) {
            break;
        } catch (const ContinueException&) {
            continue;
        }
    }
}

void Interpreter::visitForStmt(ForStmt* stmt) {
    // 创建 for 循环环境
    auto previous = environment_;
    environment_ = environment_->createChild();
    
    // 初始化
    if (stmt->initializer) {
        execute(stmt->initializer);
    }
    
    // 循环
    while (true) {
        if (stmt->condition) {
            if (!evaluate(stmt->condition).isTruthy()) {
                break;
            }
        }
        
        try {
            executeBlock(stmt->body, environment_->createChild());
        } catch (const BreakException&) {
            break;
        } catch (const ContinueException&) {
            // 继续执行 increment
        }
        
        if (stmt->increment) {
            evaluate(stmt->increment);
        }
    }
    
    environment_ = previous;
}

void Interpreter::visitForInStmt(ForInStmt* stmt) {
    LoongValue iterable = evaluate(stmt->iterable);
    
    if (!iterable.isList() && !iterable.isString()) {
        LOONG_TYPE_ERROR("for-in 循环只能遍历列表或字符串");
    }
    
    if (iterable.isList()) {
        for (const auto& item : iterable.listValue) {
            auto loopEnv = environment_->createChild();
            loopEnv->define(stmt->varName, item);
            
            try {
                executeBlock(stmt->body, loopEnv);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
    } else if (iterable.isString()) {
        for (char c : iterable.stringValue) {
            auto loopEnv = environment_->createChild();
            loopEnv->define(stmt->varName, LoongValue::string(std::string(1, c)));
            
            try {
                executeBlock(stmt->body, loopEnv);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
    }
}

void Interpreter::visitBreakStmt(BreakStmt*) {
    throw BreakException();
}

void Interpreter::visitContinueStmt(ContinueStmt*) {
    throw ContinueException();
}

void Interpreter::visitImportStmt(ImportStmt* stmt) {
    if (loadedModules_.count(stmt->moduleName)) {
        return; // 已加载
    }
    
    LoongValue moduleObj = loadModule(stmt->moduleName, stmt->filePath);
    if (!moduleObj.isNil()) {
        // 提取变量名：点分模块名使用最后一段，如 dragonfire.drivers.driver → driver
        std::string varName = stmt->moduleName;
        size_t lastDot = varName.rfind('.');
        if (lastDot != std::string::npos) {
            varName = varName.substr(lastDot + 1);
        }
        // 将模块对象注册到当前环境
        environment_->define(varName, moduleObj);
        loadedModules_.insert(stmt->moduleName);
    }
}

void Interpreter::visitTryStmt(TryStmt* stmt) {
    bool hasException = false;
    LoongValue exceptionValue;
    
    try {
        // 执行try块
        for (const auto& s : stmt->tryBlock) {
            execute(s);
        }
    } catch (const LoongException& e) {
        hasException = true;
        exceptionValue = e.value;
    } catch (const std::runtime_error& e) {
        // 捕获运行时错误，转换为Loong异常
        hasException = true;
        exceptionValue = LoongValue::string(e.what());
    }
    
    // 执行catch块
    if (hasException && !stmt->catchBlock.empty()) {
        // 创建新的环境
        auto catchEnv = std::make_shared<Environment>(environment_);
        if (!stmt->catchVar.empty()) {
            catchEnv->define(stmt->catchVar, exceptionValue);
        }
        
        auto previous = environment_;
        try {
            environment_ = catchEnv;
            for (const auto& s : stmt->catchBlock) {
                execute(s);
            }
        } catch (...) {
            environment_ = previous;
            throw; // 重新抛出
        }
        environment_ = previous;
    }
    
    // 执行finally块
    if (!stmt->finallyBlock.empty()) {
        for (const auto& s : stmt->finallyBlock) {
            execute(s);
        }
    }
    
    // 如果没有catch块且有异常，重新抛出
    if (hasException && stmt->catchBlock.empty()) {
        throw LoongException(exceptionValue);
    }
}

void Interpreter::visitThrowStmt(ThrowStmt* stmt) {
    LoongValue value = evaluate(stmt->value);
    throw LoongException(value, stmt->line, stmt->column);
}

void Interpreter::visitClassStmt(ClassStmt* stmt) {
    // 创建类对象
    auto klass = std::make_shared<LoongClass>(stmt->name, stmt->superClass);
    klass->closure = environment_;
    
    // 收集方法
    for (const auto& methodStmt : stmt->body) {
        if (auto fnStmt = dynamic_cast<FnStmt*>(methodStmt.get())) {
            auto method = std::make_shared<UserFunction>();
            method->name = fnStmt->name;
            method->params = fnStmt->params;
            // 转换defaultParams
            for (const auto& dp : fnStmt->defaultParams) {
                method->defaultParams.push_back(DefaultParamInfo(dp.first, dp.second));
            }
            method->body = fnStmt->body;
            method->closure = environment_;  // 保存定义时的环境
            
            LoongValue methodValue = LoongValue::userFunction(method);
            klass->methods[fnStmt->name] = methodValue;
        }
    }
    
    // 注册类
    environment_->define(stmt->name, LoongValue::classVal(klass));
}

void Interpreter::visitSwitchStmt(SwitchStmt* stmt) {
    // 计算switch表达式的值
    LoongValue switchValue = evaluate(stmt->expression);
    
    bool matched = false;
    bool hasDefault = false;
    const CaseClause* defaultClause = nullptr;
    
    // 查找匹配的case或default
    for (const auto& caseClause : stmt->cases) {
        if (caseClause.isDefault) {
            hasDefault = true;
            defaultClause = &caseClause;
            continue;
        }
        
        // 计算case值
        LoongValue caseValue = evaluate(caseClause.value);
        
        // 比较switch值和case值
        bool isEqual = false;
        if (switchValue.type == caseValue.type) {
            if (switchValue.isNumber()) {
                isEqual = (switchValue.numberValue == caseValue.numberValue);
            } else if (switchValue.isBigint()) {
                isEqual = (switchValue.bigintValue == caseValue.bigintValue);
            } else if (switchValue.isString()) {
                isEqual = (switchValue.stringValue == caseValue.stringValue);
            } else if (switchValue.isChar()) {
                isEqual = (switchValue.charValue == caseValue.charValue);
            } else if (switchValue.isBool()) {
                isEqual = (switchValue.boolValue == caseValue.boolValue);
            } else if (switchValue.isNil()) {
                isEqual = true;
            }
        }
        
        if (isEqual) {
            matched = true;
            // 执行匹配的case体
            executeBlock(caseClause.body, environment_->createChild());
            break;
        }
    }
    
    // 如果没有匹配的case但有default，执行default
    if (!matched && hasDefault && defaultClause) {
        executeBlock(defaultClause->body, environment_->createChild());
    }
}

void Interpreter::visitSpawnStmt(SpawnStmt* stmt) {
    auto env = environment_;
    auto expr = stmt->expression;
    auto globals = globals_;
    auto currentDir = currentDirectory_;
    auto sourceFile = currentSourceFile_;
    auto execPath = executablePath_;
    
    std::thread t([env, expr, globals, currentDir, sourceFile, execPath]() {
        try {
            Interpreter threadInterpreter;
            threadInterpreter.environment_ = env;
            threadInterpreter.globals_ = globals;
            threadInterpreter.setCurrentDirectory(currentDir);
            threadInterpreter.setSourceFile(sourceFile);
            threadInterpreter.setExecutablePath(execPath);
            
            // Evaluate the expression (could be a lambda or function call)
            LoongValue result = threadInterpreter.evaluate(expr);
            
            // If it's a function value, call it
            if (result.isUserFunction()) {
                auto func = result.userFunc;
                auto previousEnv = threadInterpreter.environment_;
                auto newEnv = func->closure->createChild();
                threadInterpreter.environment_ = newEnv;
                
                // Bind parameters (no arguments for spawn)
                for (size_t i = 0; i < func->params.size(); i++) {
                    threadInterpreter.environment_->define(func->params[i], LoongValue::nil());
                }
                
                // Bind default parameters
                for (size_t i = 0; i < func->defaultParams.size(); i++) {
                    threadInterpreter.environment_->define(
                        func->defaultParams[i].name, 
                        threadInterpreter.evaluate(func->defaultParams[i].defaultValue)
                    );
                }
                
                // Execute function body
                try {
                    for (const auto& stmt : func->body) {
                        threadInterpreter.execute(stmt);
                    }
                } catch (const ReturnException&) {
                    // Function returned, ignore
                }
                
                threadInterpreter.environment_ = previousEnv;
            }
        } catch (const std::exception& e) {
            std::cerr << "Spawn任务异常: " << e.what() << std::endl;
        }
    });
    
    std::lock_guard<std::mutex> lock(threadsMutex_);
    threads_.push_back(std::move(t));
}

void Interpreter::visitLockStmt(LockStmt* stmt) {
    LoongValue mutexVal = evaluate(stmt->mutex);
    if (!mutexVal.isMutex()) {
        throw std::runtime_error("lock语句需要一个mutex对象");
    }
    
    auto mutex = mutexVal.mutexValue;
    mutex->lock();
    try {
        executeBlock(stmt->body, environment_->createChild());
    } catch (...) {
        mutex->unlock();
        throw;
    }
    mutex->unlock();
}

LoongValue Interpreter::visitChannelSendExpr(ChannelSendExpr* expr) {
    LoongValue channelVal = evaluate(expr->channel);
    if (!channelVal.isChannel()) {
        throw std::runtime_error("send方法需要一个channel对象");
    }
    
    LoongValue value = evaluate(expr->value);
    channelVal.channelValue->send(value);
    return LoongValue::nil();
}

LoongValue Interpreter::visitChannelRecvExpr(ChannelRecvExpr* expr) {
    LoongValue channelVal = evaluate(expr->channel);
    if (!channelVal.isChannel()) {
        throw std::runtime_error("recv方法需要一个channel对象");
    }
    
    return channelVal.channelValue->recv();
}

LoongValue Interpreter::visitMutexLockExpr(MutexLockExpr* expr) {
    LoongValue mutexVal = evaluate(expr->mutex);
    if (!mutexVal.isMutex()) {
        throw std::runtime_error("lock方法需要一个mutex对象");
    }
    
    mutexVal.mutexValue->lock();
    return LoongValue::nil();
}

LoongValue Interpreter::visitMutexUnlockExpr(MutexUnlockExpr* expr) {
    LoongValue mutexVal = evaluate(expr->mutex);
    if (!mutexVal.isMutex()) {
        throw std::runtime_error("unlock方法需要一个mutex对象");
    }
    
    mutexVal.mutexValue->unlock();
    return LoongValue::nil();
}

LoongValue Interpreter::visitThisExpr(ThisExpr* expr) {
    if (currentInstance_.isNil()) {
        throw std::runtime_error("\'this\' 只能在方法内部使用");
    }
    return currentInstance_;
}

LoongValue Interpreter::visitSuperExpr(SuperExpr* expr) {
    if (!currentSuperClass_) {
        throw std::runtime_error("'super' 只能在子类方法内使用");
    }
    
    // 查找父类方法
    auto methodIt = currentSuperClass_->methods.find(expr->method);
    if (methodIt == currentSuperClass_->methods.end()) {
        throw std::runtime_error("未定义的方法: " + expr->method);
    }
    
    return methodIt->second;
}

LoongValue Interpreter::visitLambdaExpr(LambdaExpr* expr) {
    // 创建用户函数对象
    auto func = std::make_shared<UserFunction>();
    func->name = "<lambda>";  // Lambda/匿名函数名称
    func->params = expr->params;
    func->closure = environment_;  // 捕获当前环境（闭包）
    
    // 转换默认参数
    for (const auto& dp : expr->defaultParams) {
        func->defaultParams.push_back(DefaultParamInfo(dp.first, dp.second));
    }
    
    // 处理函数体
    if (expr->isBlockBody) {
        // 块体 Lambda/匿名函数
        func->body = expr->body;
    } else {
        // 表达式体 Lambda - 需要自动返回
        // 创建一个 return 语句包装表达式
        auto returnStmt = ReturnStmt::create(expr->expression, 
                                             expr->expression->line, 
                                             expr->expression->column);
        func->body = {returnStmt};
    }
    
    return LoongValue::userFunction(func);
}

LoongValue Interpreter::visitTernaryExpr(TernaryExpr* expr) {
    LoongValue conditionValue = evaluate(expr->condition);
    
    if (conditionValue.isTruthy()) {
        return evaluate(expr->thenExpr);
    } else {
        return evaluate(expr->elseExpr);
    }
}

// 获取父目录
std::string Interpreter::getParentDirectory(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos || pos == 0) {
        return "";
    }
    return path.substr(0, pos);
}

// 判断路径是否为绝对路径
static bool isAbsolutePath(const std::string& path) {
    if (path.empty()) return false;
    // Windows绝对路径: C:\... 或 C:/...
    if (path.size() >= 2 && path[1] == ':') return true;
    // Unix绝对路径: /...
    if (path[0] == '/' || path[0] == '\\') return true;
    return false;
}

// 判断路径是否以.或..开头（相对路径标记）
static bool startsWithDotRelative(const std::string& path) {
    if (path.empty()) return false;
    if (path[0] != '.') return false;
    // ./path 或 .\path
    if (path.size() >= 2 && (path[1] == '/' || path[1] == '\\')) return true;
    // ../path 或 ..\path
    if (path.size() >= 3 && path[1] == '.' && (path[2] == '/' || path[2] == '\\')) return true;
    return false;
}

// 解析模块路径
std::string Interpreter::resolveModulePath(const std::string& moduleName, const std::string& filePath) {
    // 获取当前源文件所在目录
    std::string sourceDir;
    if (!importStack_.empty()) {
        sourceDir = getParentDirectory(importStack_.back());
    } else if (!currentSourceFile_.empty()) {
        sourceDir = getParentDirectory(currentSourceFile_);
    }
    
    // 情况1: import moduleName from "path" (指定了明确路径)
    if (!filePath.empty()) {
        // 绝对路径直接使用
        if (isAbsolutePath(filePath)) {
            std::ifstream test(filePath);
            if (test.is_open()) {
                test.close();
                return filePath;
            }
            return "";
        }
        
        // 检查路径是否以 . 或 .. 开头
        bool isDotRelative = startsWithDotRelative(filePath);
        
        // 相对路径：基于当前源文件目录解析
        if (!sourceDir.empty()) {
            std::filesystem::path base(sourceDir);
            std::filesystem::path resolved = base / filePath;
            std::string fullPath = resolved.string();
            std::ifstream test(fullPath);
            if (test.is_open()) {
                test.close();
                return fullPath;
            }
        } else {
            // 没有源文件目录时，直接尝试相对路径
            std::ifstream test(filePath);
            if (test.is_open()) {
                test.close();
                return filePath;
            }
        }
        
        // 如果路径以 . 或 .. 开头，只在源文件目录下寻址，不搜索std
        if (isDotRelative) {
            return "";
        }
        
        // 普通相对路径：尝试从 loong.exe 安装目录下的 std 寻址
        // 同时支持开发环境: 向上查找exe目录的父目录中是否有std/
        if (!executablePath_.empty()) {
            // 先检查exe目录/std/
            std::filesystem::path stdPath(std::filesystem::path(executablePath_) / "std");
            std::filesystem::path resolved = stdPath / filePath;
            std::string fullPath = resolved.string();
            std::ifstream test(fullPath);
            if (test.is_open()) {
                test.close();
                return fullPath;
            }
            
            // 开发环境回退: 向上查找父目录中的std/
            std::string searchDir = executablePath_;
            for (int i = 0; i < 5; ++i) {
                std::string parent = getParentDirectory(searchDir);
                if (parent.empty() || parent == searchDir) break;
                
                std::filesystem::path parentStdPath(std::filesystem::path(parent) / "std");
                resolved = parentStdPath / filePath;
                fullPath = resolved.string();
                std::ifstream testParent(fullPath);
                if (testParent.is_open()) {
                    testParent.close();
                    return fullPath;
                }
                searchDir = parent;
            }
        }
        
        return "";
    }
    
    // 情况2: import moduleName (自动寻址)
    // 将点分模块名转换为路径: dragonfire.drivers.driver → dragonfire/drivers/driver
    std::string modulePath = moduleName;
    std::replace(modulePath.begin(), modulePath.end(), '.', '/');
    std::string moduleFile = modulePath + ".loong";
    
    // 1. 优先从当前源文件所在目录下寻址
    if (!sourceDir.empty()) {
        std::filesystem::path fullPath = std::filesystem::path(sourceDir) / moduleFile;
        std::string fullPathStr = fullPath.string();
        std::ifstream test(fullPathStr);
        if (test.is_open()) {
            test.close();
            return fullPathStr;
        }
    }
    
    // 2. 如果没有找到，从loong.exe安装目录下的std寻址
    // 同时支持开发环境: 向上查找exe目录的父目录中是否有std/
    if (!executablePath_.empty()) {
        // 先检查exe目录/std/
        std::filesystem::path stdPath(std::filesystem::path(executablePath_) / "std");
        std::filesystem::path fullPath = stdPath / moduleFile;
        std::string fullPathStr = fullPath.string();
        std::ifstream test(fullPathStr);
        if (test.is_open()) {
            test.close();
            return fullPathStr;
        }
        
        // 开发环境回退: 向上查找父目录中的std/
        std::string searchDir = executablePath_;
        for (int i = 0; i < 5; ++i) {  // 最多向上5层
            std::string parent = getParentDirectory(searchDir);
            if (parent.empty() || parent == searchDir) break;
            
            std::filesystem::path parentStdPath(std::filesystem::path(parent) / "std");
            fullPath = parentStdPath / moduleFile;
            fullPathStr = fullPath.string();
            std::ifstream testParent(fullPathStr);
            if (testParent.is_open()) {
                testParent.close();
                return fullPathStr;
            }
            searchDir = parent;
        }
    }
    
    return "";
}

LoongValue Interpreter::loadModule(const std::string& moduleName, const std::string& filePath) {
    // 检查是否已加载
    if (loadedModules_.find(moduleName) != loadedModules_.end()) {
        // 返回nil表示已加载过，调用者不需要再注册
        return LoongValue::nil();
    }
    
    // 解析模块路径
    std::string path = resolveModulePath(moduleName, filePath);
    
    if (path.empty()) {
        LOONG_IMPORT_ERROR("无法找到模块: " + moduleName);
    }
    
    // 读取文件
    std::ifstream file(path);
    if (!file.is_open()) {
        LOONG_IMPORT_ERROR("无法加载模块: " + moduleName + " (路径: " + path + ")");
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();
    
    // 记录当前源文件（用于嵌套导入）
    std::string previousSourceFile = currentSourceFile_;
    currentSourceFile_ = path;
    importStack_.push_back(path);
    
    // 词法分析
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    
    // 语法分析
    Parser parser(tokens);
    Program program = parser.parse();
    
    if (parser.hadError()) {
        currentSourceFile_ = previousSourceFile;
        importStack_.pop_back();
        LOONG_IMPORT_ERROR("模块 " + moduleName + " 语法错误: " + parser.getErrorMessage());
    }
    
    // 创建模块专属环境，以globals_为父环境
    auto moduleEnv = std::make_shared<Environment>(globals_);
    auto previous = environment_;
    environment_ = moduleEnv;
    
    try {
        // 执行模块代码
        for (const auto& stmt : program.statements) {
            execute(stmt);
        }
    } catch (const LoongError& e) {
        environment_ = previous;
        currentSourceFile_ = previousSourceFile;
        importStack_.pop_back();
        throw;
    }
    
    // 收集模块中定义的变量和函数，打包成Dict
    std::map<std::string, LoongValue> moduleMembers;
    for (const auto& [name, value] : moduleEnv->getValues()) {
        // 只过滤掉内置函数（类型为BUILTIN_FUNCTION）
        // 允许模块导出与全局变量同名的变量（如PI、E等常量）
        if (value.type != ValueType::BUILTIN_FUNCTION) {
            moduleMembers[name] = value;
        }
    }
    
    environment_ = previous;
    currentSourceFile_ = previousSourceFile;
    importStack_.pop_back();
    loadedModules_.insert(moduleName);
    
    return LoongValue::dict(moduleMembers);
}

void Interpreter::interpret(const Program& program) {
    for (const auto& stmt : program.statements) {
        execute(stmt);
    }
    
    // 等待所有spawn线程完成
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

// ==================== 内置函数 ====================

void Interpreter::registerBuiltinFunctions() {
    // print - 打印不换行
    globals_->define("print", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) {
            for (const auto& arg : args) {
                std::cout << arg.toString();
            }
            return LoongValue::nil();
        }
    ));
    
    // println - 打印换行
    globals_->define("println", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) {
            for (const auto& arg : args) {
                std::cout << arg.toString();
            }
            std::cout << std::endl;
            return LoongValue::nil();
        }
    ));
    
    // input - 读取输入
    globals_->define("input", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) {
            if (!args.empty()) {
                std::cout << args[0].toString();
            }
            std::string line;
            std::getline(std::cin, line);
            return LoongValue::string(line);
        }
    ));
    
    // len - 获取长度
    globals_->define("len", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) {
                throw std::runtime_error("len() 需要一个参数");
            }
            const auto& arg = args[0];
            if (arg.isString()) {
                return LoongValue::number(arg.stringValue.length());
            }
            if (arg.isList()) {
                return LoongValue::number(arg.listValue.size());
            }
            if (arg.isDict()) {
                return LoongValue::number(arg.dictValue.size());
            }
            throw std::runtime_error("len() 参数必须是字符串、列表或字典");
        }
    ));
    
    // type - 获取类型
    globals_->define("type", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) {
                return LoongValue::string("nil");
            }
            return LoongValue::string(args[0].typeName());
        }
    ));
    
    // str - 转字符串
    globals_->define("str", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) {
                return LoongValue::string("nil");
            }
            return LoongValue::string(args[0].toString());
        }
    ));
    
    // num - 转数字
    globals_->define("num", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) {
                return LoongValue::number(0);
            }
            if (args[0].isNumber()) {
                return args[0];
            }
            if (args[0].isString()) {
                try {
                    return LoongValue::number(std::stod(args[0].stringValue));
                } catch (...) {
                    throw std::runtime_error("无法将字符串转换为数字: " + args[0].stringValue);
                }
            }
            throw std::runtime_error("无法将 " + args[0].typeName() + " 转换为数字");
        }
    ));
    
    // push - 列表添加元素（返回新列表）
    globals_->define("push", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2) {
                throw std::runtime_error("push() 需要两个参数");
            }
            if (!args[0].isList()) {
                throw std::runtime_error("push() 第一个参数必须是列表");
            }
            auto list = args[0].listValue;
            list.push_back(args[1]);
            return LoongValue::list(list);
        }
    ));
    
    // pop - 列表弹出元素（返回新列表）
    globals_->define("pop", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isList()) {
                throw std::runtime_error("pop() 需要一个列表参数");
            }
            auto list = args[0].listValue;
            if (list.empty()) {
                throw std::runtime_error("无法从空列表弹出元素");
            }
            return list.back();
        }
    ));
    
    // ========== 字符串函数 ==========
    
    // split - 分割字符串
    globals_->define("split", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("split() 需要两个字符串参数");
            }
            std::vector<LoongValue> result;
            std::string str = args[0].stringValue;
            std::string sep = args[1].stringValue;
            if (sep.empty()) {
                for (char c : str) {
                    result.push_back(LoongValue::string(std::string(1, c)));
                }
                return LoongValue::list(result);
            }
            size_t pos = 0, prev = 0;
            while ((pos = str.find(sep, prev)) != std::string::npos) {
                result.push_back(LoongValue::string(str.substr(prev, pos - prev)));
                prev = pos + sep.length();
            }
            result.push_back(LoongValue::string(str.substr(prev)));
            return LoongValue::list(result);
        }
    ));
    
    // join - 连接列表为字符串
    globals_->define("join", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isList() || !args[1].isString()) {
                throw std::runtime_error("join() 需要一个列表和一个分隔符");
            }
            std::string result;
            for (size_t i = 0; i < args[0].listValue.size(); i++) {
                if (i > 0) result += args[1].stringValue;
                result += args[0].listValue[i].toString();
            }
            return LoongValue::string(result);
        }
    ));
    
    // trim - 去除首尾空白
    globals_->define("trim", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("trim() 需要一个字符串参数");
            }
            std::string s = args[0].stringValue;
            size_t start = s.find_first_not_of(" \t\n\r");
            if (start == std::string::npos) return LoongValue::string("");
            size_t end = s.find_last_not_of(" \t\n\r");
            return LoongValue::string(s.substr(start, end - start + 1));
        }
    ));
    
    // replace - 替换子串
    globals_->define("replace", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 3 || !args[0].isString() || !args[1].isString() || !args[2].isString()) {
                throw std::runtime_error("replace() 需要三个字符串参数");
            }
            std::string result = args[0].stringValue;
            std::string oldStr = args[1].stringValue;
            std::string newStr = args[2].stringValue;
            size_t pos = 0;
            while ((pos = result.find(oldStr, pos)) != std::string::npos) {
                result.replace(pos, oldStr.length(), newStr);
                pos += newStr.length();
            }
            return LoongValue::string(result);
        }
    ));
    
    // upper - 转大写
    globals_->define("upper", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("upper() 需要一个字符串参数");
            }
            std::string s = args[0].stringValue;
            for (char& c : s) c = std::toupper(c);
            return LoongValue::string(s);
        }
    ));
    
    // lower - 转小写
    globals_->define("lower", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("lower() 需要一个字符串参数");
            }
            std::string s = args[0].stringValue;
            for (char& c : s) c = std::tolower(c);
            return LoongValue::string(s);
        }
    ));
    
    // startswith - 检查前缀
    globals_->define("startswith", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("startswith() 需要两个字符串参数");
            }
            std::string s = args[0].stringValue;
            std::string prefix = args[1].stringValue;
            if (prefix.length() > s.length()) return LoongValue::boolean(false);
            return LoongValue::boolean(s.substr(0, prefix.length()) == prefix);
        }
    ));
    
    // endswith - 检查后缀
    globals_->define("endswith", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("endswith() 需要两个字符串参数");
            }
            std::string s = args[0].stringValue;
            std::string suffix = args[1].stringValue;
            if (suffix.length() > s.length()) return LoongValue::boolean(false);
            return LoongValue::boolean(s.substr(s.length() - suffix.length()) == suffix);
        }
    ));
    
    // find - 查找子串位置
    globals_->define("find", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("find() 需要两个字符串参数");
            }
            size_t pos = args[0].stringValue.find(args[1].stringValue);
            if (pos == std::string::npos) return LoongValue::number(-1);
            return LoongValue::number(static_cast<double>(pos));
        }
    ));
    
    // substr - 截取子串
    globals_->define("substr", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isNumber()) {
                throw std::runtime_error("substr() 需要字符串和起始位置");
            }
            std::string s = args[0].stringValue;
            int start = static_cast<int>(args[1].numberValue);
            if (start < 0) start = 0;
            if (start >= static_cast<int>(s.length())) return LoongValue::string("");
            if (args.size() >= 3 && args[2].isNumber()) {
                int len = static_cast<int>(args[2].numberValue);
                return LoongValue::string(s.substr(start, len));
            }
            return LoongValue::string(s.substr(start));
        }
    ));
    
    // abs - 绝对值
    globals_->define("abs", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("abs() 需要一个数字参数");
            }
            return LoongValue::number(std::abs(args[0].numberValue));
        }
    ));
    
    // sqrt - 平方根
    globals_->define("sqrt", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("sqrt() 需要一个数字参数");
            }
            return LoongValue::number(std::sqrt(args[0].numberValue));
        }
    ));
    
    // floor - 向下取整
    globals_->define("floor", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("floor() 需要一个数字参数");
            }
            return LoongValue::number(std::floor(args[0].numberValue));
        }
    ));
    
    // ceil - 向上取整
    globals_->define("ceil", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("ceil() 需要一个数字参数");
            }
            return LoongValue::number(std::ceil(args[0].numberValue));
        }
    ));
    
    // range - 生成范围
    globals_->define("range", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            std::vector<LoongValue> result;
            
            if (args.size() == 1) {
                // range(n) -> 0 to n-1
                int end = static_cast<int>(args[0].numberValue);
                for (int i = 0; i < end; i++) {
                    result.push_back(LoongValue::number(i));
                }
            } else if (args.size() == 2) {
                // range(start, end)
                int start = static_cast<int>(args[0].numberValue);
                int end = static_cast<int>(args[1].numberValue);
                for (int i = start; i < end; i++) {
                    result.push_back(LoongValue::number(i));
                }
            } else if (args.size() >= 3) {
                // range(start, end, step)
                int start = static_cast<int>(args[0].numberValue);
                int end = static_cast<int>(args[1].numberValue);
                int step = static_cast<int>(args[2].numberValue);
                if (step > 0) {
                    for (int i = start; i < end; i += step) {
                        result.push_back(LoongValue::number(i));
                    }
                } else if (step < 0) {
                    for (int i = start; i > end; i += step) {
                        result.push_back(LoongValue::number(i));
                    }
                }
            }
            
            return LoongValue::list(result);
        }
    ));
    
    // ========== 数学函数 ==========
    
    // round - 四舍五入
    globals_->define("round", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("round() 需要一个数字参数");
            }
            return LoongValue::number(std::round(args[0].numberValue));
        }
    ));
    
    // pow - 幂运算
    globals_->define("pow", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isNumber() || !args[1].isNumber()) {
                throw std::runtime_error("pow() 需要两个数字参数");
            }
            return LoongValue::number(std::pow(args[0].numberValue, args[1].numberValue));
        }
    ));
    
    // log - 自然对数
    globals_->define("log", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("log() 需要一个数字参数");
            }
            return LoongValue::number(std::log(args[0].numberValue));
        }
    ));
    
    // log10 - 以10为底对数
    globals_->define("log10", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("log10() 需要一个数字参数");
            }
            return LoongValue::number(std::log10(args[0].numberValue));
        }
    ));
    
    // sin - 正弦
    globals_->define("sin", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("sin() 需要一个数字参数");
            }
            return LoongValue::number(std::sin(args[0].numberValue));
        }
    ));
    
    // cos - 余弦
    globals_->define("cos", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("cos() 需要一个数字参数");
            }
            return LoongValue::number(std::cos(args[0].numberValue));
        }
    ));
    
    // tan - 正切
    globals_->define("tan", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("tan() 需要一个数字参数");
            }
            return LoongValue::number(std::tan(args[0].numberValue));
        }
    ));
    
    // asin - 反正弦
    globals_->define("asin", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("asin() 需要一个数字参数");
            }
            return LoongValue::number(std::asin(args[0].numberValue));
        }
    ));
    
    // acos - 反余弦
    globals_->define("acos", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("acos() 需要一个数字参数");
            }
            return LoongValue::number(std::acos(args[0].numberValue));
        }
    ));
    
    // atan - 反正切
    globals_->define("atan", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("atan() 需要一个数字参数");
            }
            return LoongValue::number(std::atan(args[0].numberValue));
        }
    ));
    
    // random - 随机数(0-1)
    globals_->define("random", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            static bool seeded = false;
            if (!seeded) {
                std::srand(static_cast<unsigned>(std::time(nullptr)));
                seeded = true;
            }
            double r = static_cast<double>(std::rand()) / RAND_MAX;
            return LoongValue::number(r);
        }
    ));
    
    // random_int - 随机整数
    globals_->define("random_int", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isNumber() || !args[1].isNumber()) {
                throw std::runtime_error("random_int() 需要两个数字参数");
            }
            static bool seeded = false;
            if (!seeded) {
                std::srand(static_cast<unsigned>(std::time(nullptr)));
                seeded = true;
            }
            int min = static_cast<int>(args[0].numberValue);
            int max = static_cast<int>(args[1].numberValue);
            if (min > max) { int tmp = min; min = max; max = tmp; }
            return LoongValue::number(min + (std::rand() % (max - min + 1)));
        }
    ));
    
    // PI - 圆周率常量
    globals_->define("PI", LoongValue::number(3.14159265358979323846));
    
    // E - 自然常数
    globals_->define("E", LoongValue::number(2.71828182845904523536));
    
    // ========== 文件IO函数 ==========
    
    // file_read - 读取文件
    globals_->define("file_read", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("file_read() 需要一个文件路径参数");
            }
            std::ifstream file(args[0].stringValue);
            if (!file.is_open()) {
                return LoongValue::nil();
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            return LoongValue::string(buffer.str());
        }
    ));
    
    // file_write - 写入文件
    globals_->define("file_write", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("file_write() 需要文件路径和内容参数");
            }
            std::ofstream file(args[0].stringValue);
            if (!file.is_open()) {
                return LoongValue::boolean(false);
            }
            file << args[1].stringValue;
            return LoongValue::boolean(true);
        }
    ));
    
    // file_append - 追加内容
    globals_->define("file_append", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("file_append() 需要文件路径和内容参数");
            }
            std::ofstream file(args[0].stringValue, std::ios::app);
            if (!file.is_open()) {
                return LoongValue::boolean(false);
            }
            file << args[1].stringValue;
            return LoongValue::boolean(true);
        }
    ));
    
    // file_exists - 检查文件是否存在
    globals_->define("file_exists", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("file_exists() 需要一个文件路径参数");
            }
            std::ifstream file(args[0].stringValue);
            return LoongValue::boolean(file.good());
        }
    ));
    
    // file_delete - 删除文件
    globals_->define("file_delete", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("file_delete() 需要一个文件路径参数");
            }
            return LoongValue::boolean(std::remove(args[0].stringValue.c_str()) == 0);
        }
    ));
    
    // ========== 时间函数 ==========
    
    // time_now - 当前时间戳
    globals_->define("time_now", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            return LoongValue::number(static_cast<double>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            ));
        }
    ));
    
    // time_ms - 当前毫秒时间戳
    globals_->define("time_ms", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            return LoongValue::number(static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            ));
        }
    ));
    
    // sleep - 休眠毫秒
    globals_->define("sleep", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("sleep() 需要一个毫秒数参数");
            }
            int ms = static_cast<int>(args[0].numberValue);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            return LoongValue::nil();
        }
    ));
    
    // ========== 列表增强函数 ==========
    
    // reverse - 反转列表
    globals_->define("reverse", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isList()) {
                throw std::runtime_error("reverse() 需要一个列表参数");
            }
            auto list = args[0].listValue;
            std::reverse(list.begin(), list.end());
            return LoongValue::list(list);
        }
    ));
    
    // slice - 列表切片
    globals_->define("slice", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isList() || !args[1].isNumber()) {
                throw std::runtime_error("slice() 需要列表和起始索引");
            }
            auto list = args[0].listValue;
            int start = static_cast<int>(args[1].numberValue);
            if (start < 0) start = 0;
            if (start >= static_cast<int>(list.size())) return LoongValue::list({});
            
            if (args.size() >= 3 && args[2].isNumber()) {
                int end = static_cast<int>(args[2].numberValue);
                if (end < 0) end = list.size();
                return LoongValue::list(std::vector<LoongValue>(list.begin() + start, list.begin() + end));
            }
            return LoongValue::list(std::vector<LoongValue>(list.begin() + start, list.end()));
        }
    ));
    
    // concat - 连接列表
    globals_->define("concat", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isList() || !args[1].isList()) {
                throw std::runtime_error("concat() 需要两个列表参数");
            }
            auto result = args[0].listValue;
            for (const auto& item : args[1].listValue) {
                result.push_back(item);
            }
            return LoongValue::list(result);
        }
    ));
    
    // insert - 插入元素
    globals_->define("insert", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 3 || !args[0].isList() || !args[1].isNumber()) {
                throw std::runtime_error("insert() 需要列表、索引和元素参数");
            }
            auto list = args[0].listValue;
            int index = static_cast<int>(args[1].numberValue);
            if (index < 0) index = 0;
            if (index > static_cast<int>(list.size())) index = list.size();
            list.insert(list.begin() + index, args[2]);
            return LoongValue::list(list);
        }
    ));
    
    // remove_at - 删除指定索引元素
    globals_->define("remove_at", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isList() || !args[1].isNumber()) {
                throw std::runtime_error("remove_at() 需要列表和索引参数");
            }
            auto list = args[0].listValue;
            int index = static_cast<int>(args[1].numberValue);
            if (index < 0 || index >= static_cast<int>(list.size())) {
                throw std::runtime_error("remove_at() 索引越界");
            }
            list.erase(list.begin() + index);
            return LoongValue::list(list);
        }
    ));
    
    // first - 获取第一个元素
    globals_->define("first", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isList()) {
                throw std::runtime_error("first() 需要一个列表参数");
            }
            if (args[0].listValue.empty()) return LoongValue::nil();
            return args[0].listValue[0];
        }
    ));
    
    // last - 获取最后一个元素
    globals_->define("last", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isList()) {
                throw std::runtime_error("last() 需要一个列表参数");
            }
            if (args[0].listValue.empty()) return LoongValue::nil();
            return args[0].listValue.back();
        }
    ));
    
    // ========== 类型判断函数 ==========
    
    // is_null - 判断是否为nil
    globals_->define("is_null", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) return LoongValue::boolean(true);
            return LoongValue::boolean(args[0].isNil());
        }
    ));
    
    // is_number - 判断是否为数字
    globals_->define("is_number", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) return LoongValue::boolean(false);
            return LoongValue::boolean(args[0].isNumber());
        }
    ));
    
    // is_string - 判断是否为字符串
    globals_->define("is_string", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) return LoongValue::boolean(false);
            return LoongValue::boolean(args[0].isString());
        }
    ));
    
    // is_list - 判断是否为列表
    globals_->define("is_list", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) return LoongValue::boolean(false);
            return LoongValue::boolean(args[0].isList());
        }
    ));
    
    // is_dict - 判断是否为字典
    globals_->define("is_dict", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) return LoongValue::boolean(false);
            return LoongValue::boolean(args[0].isDict());
        }
    ));
    
    // is_function - 判断是否为函数
    globals_->define("is_function", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) return LoongValue::boolean(false);
            return LoongValue::boolean(args[0].isFunction());
        }
    ));
    
    // bool - 转布尔值
    globals_->define("bool", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) return LoongValue::boolean(false);
            return LoongValue::boolean(args[0].isTruthy());
        }
    ));
    
    // int - 转整数
    globals_->define("int", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) return LoongValue::number(0);
            if (args[0].isNumber()) {
                return LoongValue::number(static_cast<double>(static_cast<long long>(args[0].numberValue)));
            }
            if (args[0].isChar()) {
                // 将字符转换为ASCII码
                return LoongValue::number(static_cast<double>(static_cast<unsigned char>(args[0].charValue)));
            }
            if (args[0].isString()) {
                try {
                    return LoongValue::number(static_cast<double>(std::stoll(args[0].stringValue)));
                } catch (...) {
                    return LoongValue::number(0);
                }
            }
            return LoongValue::number(0);
        }
    ));
    
    // chr - 将整数转换为ASCII字符
    globals_->define("chr", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty()) {
                throw std::runtime_error("chr() 需要一个整数参数");
            }
            int code = 0;
            if (args[0].isNumber()) {
                code = static_cast<int>(args[0].numberValue);
            } else if (args[0].isBigint()) {
                code = static_cast<int>(args[0].bigintValue.toDouble());
            } else {
                throw std::runtime_error("chr() 参数必须是整数");
            }
            // 检查是否在有效范围内
            if (code < 0 || code > 127) {
                throw std::runtime_error("chr() 参数必须在0-127范围内");
            }
            return LoongValue::charVal(static_cast<char>(code));
        }
    ));
    
    // ========== HTTP网络请求函数 ==========
    
    // http_get - GET请求
    globals_->define("http_get", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("http_get() 需要一个URL参数");
            }
            std::string url = args[0].stringValue;
            std::map<std::string, std::string> headers;
            
            // 可选的headers参数
            if (args.size() > 1 && args[1].isDict()) {
                for (const auto& h : args[1].dictValue) {
                    headers[h.first] = h.second.toString();
                }
            }
            
            HttpResponse resp = HttpClient::get(url, headers);
            
            // 返回结果字典
            std::map<std::string, LoongValue> result;
            result["status"] = LoongValue::number(resp.statusCode);
            result["body"] = LoongValue::string(resp.body);
            result["success"] = LoongValue::boolean(resp.success);
            result["error"] = LoongValue::string(resp.error);
            
            // 响应头
            std::map<std::string, LoongValue> respHeaders;
            for (const auto& h : resp.headers) {
                respHeaders[h.first] = LoongValue::string(h.second);
            }
            result["headers"] = LoongValue::dict(respHeaders);
            
            return LoongValue::dict(result);
        }
    ));
    
    // http_post - POST请求
    globals_->define("http_post", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("http_post() 需要URL参数");
            }
            std::string url = args[0].stringValue;
            std::string body = args.size() > 1 && args[1].isString() ? args[1].stringValue : "";
            std::map<std::string, std::string> headers;
            
            // 可选的headers参数
            if (args.size() > 2 && args[2].isDict()) {
                for (const auto& h : args[2].dictValue) {
                    headers[h.first] = h.second.toString();
                }
            }
            
            HttpResponse resp = HttpClient::post(url, body, headers);
            
            std::map<std::string, LoongValue> result;
            result["status"] = LoongValue::number(resp.statusCode);
            result["body"] = LoongValue::string(resp.body);
            result["success"] = LoongValue::boolean(resp.success);
            result["error"] = LoongValue::string(resp.error);
            
            std::map<std::string, LoongValue> respHeaders;
            for (const auto& h : resp.headers) {
                respHeaders[h.first] = LoongValue::string(h.second);
            }
            result["headers"] = LoongValue::dict(respHeaders);
            
            return LoongValue::dict(result);
        }
    ));
    
    // http_request - 通用HTTP请求
    globals_->define("http_request", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("http_request() 需要URL和method参数");
            }
            std::string url = args[0].stringValue;
            std::string method = args[1].stringValue;
            std::string body = args.size() > 2 && args[2].isString() ? args[2].stringValue : "";
            std::map<std::string, std::string> headers;
            
            if (args.size() > 3 && args[3].isDict()) {
                for (const auto& h : args[3].dictValue) {
                    headers[h.first] = h.second.toString();
                }
            }
            
            HttpResponse resp = HttpClient::request(url, method, body, headers);
            
            std::map<std::string, LoongValue> result;
            result["status"] = LoongValue::number(resp.statusCode);
            result["body"] = LoongValue::string(resp.body);
            result["success"] = LoongValue::boolean(resp.success);
            result["error"] = LoongValue::string(resp.error);
            
            std::map<std::string, LoongValue> respHeaders;
            for (const auto& h : resp.headers) {
                respHeaders[h.first] = LoongValue::string(h.second);
            }
            result["headers"] = LoongValue::dict(respHeaders);
            
            return LoongValue::dict(result);
        }
    ));
    
    // ========== HTML解析函数 ==========
    
    // html_tag - 提取所有指定标签内容
    globals_->define("html_tag", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("html_tag() 需要HTML和标签名参数");
            }
            std::string html = args[0].stringValue;
            std::string tag = args[1].stringValue;
            std::vector<LoongValue> results;
            
            std::string openTag = "<" + tag;
            std::string closeTag = "</" + tag + ">";
            size_t pos = 0;
            
            while ((pos = html.find(openTag, pos)) != std::string::npos) {
                size_t tagEnd = html.find('>', pos);
                if (tagEnd == std::string::npos) break;
                
                size_t contentStart = tagEnd + 1;
                size_t closePos = html.find(closeTag, contentStart);
                if (closePos == std::string::npos) break;
                
                std::string content = html.substr(contentStart, closePos - contentStart);
                results.push_back(LoongValue::string(content));
                pos = closePos + closeTag.length();
            }
            
            return LoongValue::list(results);
        }
    ));
    
    // html_attr - 提取标签属性
    globals_->define("html_attr", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("html_attr() 需要HTML片段和属性名参数");
            }
            std::string html = args[0].stringValue;
            std::string attr = args[1].stringValue;
            
            std::string pattern1 = attr + "=\"";
            std::string pattern2 = attr + "='";
            
            size_t pos = html.find(pattern1);
            char quote = '"';
            if (pos == std::string::npos) {
                pos = html.find(pattern2);
                quote = '\'';
            }
            
            if (pos == std::string::npos) return LoongValue::nil();
            
            size_t start = pos + attr.length() + 2;
            size_t end = html.find(quote, start);
            if (end == std::string::npos) return LoongValue::nil();
            
            return LoongValue::string(html.substr(start, end - start));
        }
    ));
    
    // html_text - 提取纯文本
    globals_->define("html_text", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("html_text() 需要HTML参数");
            }
            std::string html = args[0].stringValue;
            std::string result;
            bool inTag = false;
            
            for (char c : html) {
                if (c == '<') {
                    inTag = true;
                } else if (c == '>') {
                    inTag = false;
                } else if (!inTag) {
                    result += c;
                }
            }
            
            // 处理HTML实体
            size_t pos = 0;
            while ((pos = result.find("&nbsp;")) != std::string::npos) {
                result.replace(pos, 6, " ");
            }
            while ((pos = result.find("&lt;")) != std::string::npos) {
                result.replace(pos, 4, "<");
            }
            while ((pos = result.find("&gt;")) != std::string::npos) {
                result.replace(pos, 4, ">");
            }
            while ((pos = result.find("&amp;")) != std::string::npos) {
                result.replace(pos, 5, "&");
            }
            
            return LoongValue::string(result);
        }
    ));
    
    // html_links - 提取所有链接
    globals_->define("html_links", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("html_links() 需要HTML参数");
            }
            std::string html = args[0].stringValue;
            std::vector<LoongValue> links;
            
            size_t pos = 0;
            while ((pos = html.find("href=", pos)) != std::string::npos) {
                char quote = html[pos + 5];
                if (quote == '"' || quote == '\'') {
                    size_t start = pos + 6;
                    size_t end = html.find(quote, start);
                    if (end != std::string::npos) {
                        links.push_back(LoongValue::string(html.substr(start, end - start)));
                        pos = end + 1;
                        continue;
                    }
                }
                pos++;
            }
            
            return LoongValue::list(links);
        }
    ));
    
    // url_encode - URL编码
    globals_->define("url_encode", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("url_encode() 需要字符串参数");
            }
            std::string s = args[0].stringValue;
            std::string result;
            
            for (char c : s) {
                if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    result += c;
                } else {
                    char buf[4];
                    sprintf(buf, "%%%02X", (unsigned char)c);
                    result += buf;
                }
            }
            
            return LoongValue::string(result);
        }
    ));
    
    // url_decode - URL解码
    globals_->define("url_decode", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("url_decode() 需要字符串参数");
            }
            std::string s = args[0].stringValue;
            std::string result;
            
            for (size_t i = 0; i < s.length(); i++) {
                if (s[i] == '%' && i + 2 < s.length()) {
                    int val;
                    sscanf(s.substr(i + 1, 2).c_str(), "%x", &val);
                    result += (char)val;
                    i += 2;
                } else if (s[i] == '+') {
                    result += ' ';
                } else {
                    result += s[i];
                }
            }
            
            return LoongValue::string(result);
        }
    ));
    
    // ==================== 网络协议函数 ====================
    
    // TCP连接存储
    static std::map<int, std::shared_ptr<Socket>> tcpSockets;
    static int nextSocketId = 1;
    
    // tcp_connect(host, port) - 创建TCP连接
    globals_->define("tcp_connect", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isNumber()) {
                throw std::runtime_error("tcp_connect() 需要 host(string) 和 port(number) 参数");
            }
            
            std::string host = args[0].stringValue;
            int port = (int)args[1].numberValue;
            
            // 获取超时参数（可选）
            int timeout = 30000;
            if (args.size() > 2 && args[2].isNumber()) {
                timeout = (int)args[2].numberValue;
            }
            
            static std::map<int, std::shared_ptr<Socket>>& sockets = tcpSockets;
            static int& nextId = nextSocketId;
            
            auto sock = std::make_shared<Socket>(SocketType::TCP);
            sock->setTimeout(timeout);
            
            if (!sock->connect(host, port)) {
                return LoongValue::number(0);
            }
            
            int id = nextId++;
            sockets[id] = sock;
            
            return LoongValue::number(id);
        }
    ));
    
    // tcp_send(socket_id, data) - 发送TCP数据
    globals_->define("tcp_send", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isNumber() || !args[1].isString()) {
                throw std::runtime_error("tcp_send() 需要 socket_id(number) 和 data(string) 参数");
            }
            
            int id = (int)args[0].numberValue;
            std::string data = args[1].stringValue;
            
            static std::map<int, std::shared_ptr<Socket>>& sockets = tcpSockets;
            
            auto it = sockets.find(id);
            if (it == sockets.end()) {
                throw std::runtime_error("无效的socket id");
            }
            
            int sent = it->second->send(data);
            return LoongValue::number(sent);
        }
    ));
    
    // tcp_recv(socket_id, size) - 接收TCP数据
    globals_->define("tcp_recv", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("tcp_recv() 需要 socket_id(number) 参数");
            }
            
            int id = (int)args[0].numberValue;
            int size = 4096;
            if (args.size() > 1 && args[1].isNumber()) {
                size = (int)args[1].numberValue;
            }
            
            static std::map<int, std::shared_ptr<Socket>>& sockets = tcpSockets;
            
            auto it = sockets.find(id);
            if (it == sockets.end()) {
                throw std::runtime_error("无效的socket id");
            }
            
            std::string data = it->second->recv(size);
            return LoongValue::string(data);
        }
    ));
    
    // tcp_recv_exact(socket_id, size) - 精确接收指定字节数的TCP数据
    globals_->define("tcp_recv_exact", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isNumber() || !args[1].isNumber()) {
                throw std::runtime_error("tcp_recv_exact() 需要 socket_id(number) 和 size(number) 参数");
            }
            
            int id = (int)args[0].numberValue;
            int size = (int)args[1].numberValue;
            
            static std::map<int, std::shared_ptr<Socket>>& sockets = tcpSockets;
            
            auto it = sockets.find(id);
            if (it == sockets.end()) {
                throw std::runtime_error("无效的socket id");
            }
            
            std::string data = it->second->recvExact(size);
            return LoongValue::string(data);
        }
    ));
    
    // tcp_recv_all(socket_id) - 接收所有TCP数据
    globals_->define("tcp_recv_all", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("tcp_recv_all() 需要 socket_id(number) 参数");
            }
            
            int id = (int)args[0].numberValue;
            int maxBytes = 1024 * 1024;
            if (args.size() > 1 && args[1].isNumber()) {
                maxBytes = (int)args[1].numberValue;
            }
            
            static std::map<int, std::shared_ptr<Socket>>& sockets = tcpSockets;
            
            auto it = sockets.find(id);
            if (it == sockets.end()) {
                throw std::runtime_error("无效的socket id");
            }
            
            std::string data = it->second->recvAll(maxBytes);
            return LoongValue::string(data);
        }
    ));
    
    // tcp_close(socket_id) - 关闭TCP连接
    globals_->define("tcp_close", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("tcp_close() 需要 socket_id(number) 参数");
            }
            
            int id = (int)args[0].numberValue;
            
            static std::map<int, std::shared_ptr<Socket>>& sockets = tcpSockets;
            
            auto it = sockets.find(id);
            if (it != sockets.end()) {
                it->second->close();
                sockets.erase(it);
                return LoongValue::boolean(true);
            }
            
            return LoongValue::boolean(false);
        }
    ));
    
    // tcp_connected(socket_id) - 检查TCP连接状态
    globals_->define("tcp_connected", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("tcp_connected() 需要 socket_id(number) 参数");
            }
            
            int id = (int)args[0].numberValue;
            
            static std::map<int, std::shared_ptr<Socket>>& sockets = tcpSockets;
            
            auto it = sockets.find(id);
            if (it != sockets.end()) {
                return LoongValue::boolean(it->second->isConnected());
            }
            
            return LoongValue::boolean(false);
        }
    ));
    
    // ==================== TCP 服务器函数 ====================
    static std::map<int, std::shared_ptr<TcpServer>> tcpServers;
    static int nextServerId = 2000;
    
    // tcp_server_create() - 创建TCP服务器
    globals_->define("tcp_server_create", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            static std::map<int, std::shared_ptr<TcpServer>>& servers = tcpServers;
            static int& nextId = nextServerId;
            
            auto server = std::make_shared<TcpServer>();
            int id = nextId++;
            servers[id] = server;
            
            return LoongValue::number(id);
        }
    ));
    
    // tcp_server_bind(server_id, port) - 绑定端口
    globals_->define("tcp_server_bind", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isNumber() || !args[1].isNumber()) {
                throw std::runtime_error("tcp_server_bind() 需要 server_id 和 port 参数");
            }
            
            int id = (int)args[0].numberValue;
            int port = (int)args[1].numberValue;
            
            static std::map<int, std::shared_ptr<TcpServer>>& servers = tcpServers;
            
            auto it = servers.find(id);
            if (it == servers.end()) {
                throw std::runtime_error("无效的server id");
            }
            
            bool success = it->second->bind(port);
            return LoongValue::boolean(success);
        }
    ));
    
    // tcp_server_listen(server_id, backlog) - 开始监听
    globals_->define("tcp_server_listen", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("tcp_server_listen() 需要 server_id 参数");
            }
            
            int id = (int)args[0].numberValue;
            int backlog = 5;
            if (args.size() > 1 && args[1].isNumber()) {
                backlog = (int)args[1].numberValue;
            }
            
            static std::map<int, std::shared_ptr<TcpServer>>& servers = tcpServers;
            
            auto it = servers.find(id);
            if (it == servers.end()) {
                throw std::runtime_error("无效的server id");
            }
            
            bool success = it->second->listen(backlog);
            return LoongValue::boolean(success);
        }
    ));
    
    // tcp_server_accept(server_id) - 接受连接，返回客户端socket_id
    globals_->define("tcp_server_accept", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("tcp_server_accept() 需要 server_id 参数");
            }
            
            int id = (int)args[0].numberValue;
            
            static std::map<int, std::shared_ptr<TcpServer>>& servers = tcpServers;
            static std::map<int, std::shared_ptr<Socket>>& sockets = tcpSockets;
            static int& nextSocketIdRef = nextSocketId;
            
            auto it = servers.find(id);
            if (it == servers.end()) {
                throw std::runtime_error("无效的server id");
            }
            
            Socket* client = it->second->accept();
            if (client == nullptr) {
                return LoongValue::number(0);
            }
            
            // 将客户端socket存入管理map
            int clientId = nextSocketIdRef++;
            sockets[clientId] = std::shared_ptr<Socket>(client);
            
            return LoongValue::number(clientId);
        }
    ));
    
    // tcp_server_is_listening(server_id) - 检查是否在监听
    globals_->define("tcp_server_is_listening", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("tcp_server_is_listening() 需要 server_id 参数");
            }
            
            int id = (int)args[0].numberValue;
            
            static std::map<int, std::shared_ptr<TcpServer>>& servers = tcpServers;
            
            auto it = servers.find(id);
            if (it != servers.end()) {
                return LoongValue::boolean(it->second->isListening());
            }
            
            return LoongValue::boolean(false);
        }
    ));
    
    // tcp_server_close(server_id) - 关闭服务器
    globals_->define("tcp_server_close", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("tcp_server_close() 需要 server_id 参数");
            }
            
            int id = (int)args[0].numberValue;
            
            static std::map<int, std::shared_ptr<TcpServer>>& servers = tcpServers;
            
            auto it = servers.find(id);
            if (it != servers.end()) {
                it->second->close();
                servers.erase(it);
                return LoongValue::boolean(true);
            }
            
            return LoongValue::boolean(false);
        }
    ));
    
    // UDP发送
    static std::map<int, std::shared_ptr<UdpClient>> udpClients;
    static int nextUdpId = 1000;
    
    // udp_open() - 创建UDP客户端
    globals_->define("udp_open", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            static std::map<int, std::shared_ptr<UdpClient>>& clients = udpClients;
            static int& nextId = nextUdpId;
            
            auto client = std::make_shared<UdpClient>();
            int id = nextId++;
            clients[id] = client;
            
            return LoongValue::number(id);
        }
    ));
    
    // udp_send(udp_id, host, port, data) - 发送UDP数据
    globals_->define("udp_send", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 4 || !args[0].isNumber() || !args[1].isString() || 
                !args[2].isNumber() || !args[3].isString()) {
                throw std::runtime_error("udp_send() 需要 udp_id, host, port, data 参数");
            }
            
            int id = (int)args[0].numberValue;
            std::string host = args[1].stringValue;
            int port = (int)args[2].numberValue;
            std::string data = args[3].stringValue;
            
            static std::map<int, std::shared_ptr<UdpClient>>& clients = udpClients;
            
            auto it = clients.find(id);
            if (it == clients.end()) {
                throw std::runtime_error("无效的udp id");
            }
            
            int sent = it->second->send(data, host, port);
            return LoongValue::number(sent);
        }
    ));
    
    // udp_recv(udp_id, size) - 接收UDP数据
    globals_->define("udp_recv", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("udp_recv() 需要 udp_id(number) 参数");
            }
            
            int id = (int)args[0].numberValue;
            int size = 4096;
            if (args.size() > 1 && args[1].isNumber()) {
                size = (int)args[1].numberValue;
            }
            
            static std::map<int, std::shared_ptr<UdpClient>>& clients = udpClients;
            
            auto it = clients.find(id);
            if (it == clients.end()) {
                throw std::runtime_error("无效的udp id");
            }
            
            std::string senderHost;
            int senderPort;
            std::string data = it->second->recv(senderHost, senderPort, size);
            
            // 返回字典 {data, host, port}
            std::map<std::string, LoongValue> result;
            result["data"] = LoongValue::string(data);
            result["host"] = LoongValue::string(senderHost);
            result["port"] = LoongValue::number(senderPort);
            
            return LoongValue::dict(result);
        }
    ));
    
    // udp_bind(udp_id, port) - 绑定UDP端口
    globals_->define("udp_bind", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isNumber() || !args[1].isNumber()) {
                throw std::runtime_error("udp_bind() 需要 udp_id 和 port 参数");
            }
            
            int id = (int)args[0].numberValue;
            int port = (int)args[1].numberValue;
            
            static std::map<int, std::shared_ptr<UdpClient>>& clients = udpClients;
            
            auto it = clients.find(id);
            if (it == clients.end()) {
                throw std::runtime_error("无效的udp id");
            }
            
            bool success = it->second->bind(port);
            return LoongValue::boolean(success);
        }
    ));
    
    // udp_close(udp_id) - 关闭UDP
    globals_->define("udp_close", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("udp_close() 需要 udp_id(number) 参数");
            }
            
            int id = (int)args[0].numberValue;
            
            static std::map<int, std::shared_ptr<UdpClient>>& clients = udpClients;
            
            auto it = clients.find(id);
            if (it != clients.end()) {
                it->second->close();
                clients.erase(it);
                return LoongValue::boolean(true);
            }
            
            return LoongValue::boolean(false);
        }
    ));
    
    // dns_resolve(hostname) - DNS解析
    globals_->define("dns_resolve", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("dns_resolve() 需要 hostname(string) 参数");
            }
            
            std::string hostname = args[0].stringValue;
            std::string ip = dnsResolve(hostname);
            
            return LoongValue::string(ip);
        }
    ));
    
    // dns_reverse(ip) - 反向DNS解析
    globals_->define("dns_reverse", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("dns_reverse() 需要 ip(string) 参数");
            }
            
            std::string ip = args[0].stringValue;
            std::string hostname = dnsReverse(ip);
            
            return LoongValue::string(hostname);
        }
    ));
    
    // port_check(host, port) - 检查端口是否开放
    globals_->define("port_check", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isNumber()) {
                throw std::runtime_error("port_check() 需要 host(string) 和 port(number) 参数");
            }
            
            std::string host = args[0].stringValue;
            int port = (int)args[1].numberValue;
            int timeout = 3000;
            if (args.size() > 2 && args[2].isNumber()) {
                timeout = (int)args[2].numberValue;
            }
            
            bool open = isPortOpen(host, port, timeout);
            return LoongValue::boolean(open);
        }
    ));
    
    // ==================== 二进制数据处理函数 ====================
    
    // bytes_len(data) - 获取二进制数据的字节长度
    globals_->define("bytes_len", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("bytes_len() 需要一个字符串参数");
            }
            return LoongValue::number(static_cast<double>(args[0].stringValue.size()));
        }
    ));
    
    // bytes_get(data, offset) - 获取二进制数据指定偏移处的字节值(0-255)
    globals_->define("bytes_get", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isNumber()) {
                throw std::runtime_error("bytes_get() 需要 data(string) 和 offset(number) 参数");
            }
            const std::string& data = args[0].stringValue;
            int offset = (int)args[1].numberValue;
            if (offset < 0 || offset >= (int)data.size()) {
                throw std::runtime_error("bytes_get() offset越界");
            }
            return LoongValue::number(static_cast<double>(static_cast<unsigned char>(data[offset])));
        }
    ));
    
    // bytes_set(data, offset, value) - 设置二进制数据指定偏移处的字节值，返回新数据
    globals_->define("bytes_set", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 3 || !args[0].isString() || !args[1].isNumber() || !args[2].isNumber()) {
                throw std::runtime_error("bytes_set() 需要 data(string), offset(number), value(number) 参数");
            }
            std::string data = args[0].stringValue;
            int offset = (int)args[1].numberValue;
            int value = (int)args[2].numberValue;
            if (offset < 0 || offset >= (int)data.size()) {
                throw std::runtime_error("bytes_set() offset越界");
            }
            data[offset] = static_cast<char>(value & 255);
            return LoongValue::string(data);
        }
    ));
    
    // bytes_get_int16(data, offset) - 读取小端16位整数
    globals_->define("bytes_get_int16", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isNumber()) {
                throw std::runtime_error("bytes_get_int16() 需要 data(string) 和 offset(number) 参数");
            }
            const std::string& data = args[0].stringValue;
            int offset = (int)args[1].numberValue;
            if (offset < 0 || offset + 2 > (int)data.size()) {
                throw std::runtime_error("bytes_get_int16() offset越界");
            }
            int value = (static_cast<unsigned char>(data[offset])) |
                        (static_cast<unsigned char>(data[offset + 1]) << 8);
            return LoongValue::number(static_cast<double>(value));
        }
    ));
    
    // bytes_get_int32(data, offset) - 读取小端32位整数
    globals_->define("bytes_get_int32", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isNumber()) {
                throw std::runtime_error("bytes_get_int32() 需要 data(string) 和 offset(number) 参数");
            }
            const std::string& data = args[0].stringValue;
            int offset = (int)args[1].numberValue;
            if (offset < 0 || offset + 4 > (int)data.size()) {
                throw std::runtime_error("bytes_get_int32() offset越界");
            }
            long long value = (static_cast<unsigned char>(data[offset])) |
                              (static_cast<unsigned char>(data[offset + 1]) << 8) |
                              (static_cast<unsigned char>(data[offset + 2]) << 16) |
                              (static_cast<long long>(static_cast<unsigned char>(data[offset + 3])) << 24);
            return LoongValue::number(static_cast<double>(value));
        }
    ));
    
    // bytes_get_int24(data, offset) - 读取小端24位整数
    globals_->define("bytes_get_int24", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isNumber()) {
                throw std::runtime_error("bytes_get_int24() 需要 data(string) 和 offset(number) 参数");
            }
            const std::string& data = args[0].stringValue;
            int offset = (int)args[1].numberValue;
            if (offset < 0 || offset + 3 > (int)data.size()) {
                throw std::runtime_error("bytes_get_int24() offset越界");
            }
            int value = (static_cast<unsigned char>(data[offset])) |
                        (static_cast<unsigned char>(data[offset + 1]) << 8) |
                        (static_cast<unsigned char>(data[offset + 2]) << 16);
            return LoongValue::number(static_cast<double>(value));
        }
    ));
    
    // bytes_from_int8(value) - 将整数转换为1字节字符串
    globals_->define("bytes_from_int8", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("bytes_from_int8() 需要一个整数参数");
            }
            int value = (int)args[0].numberValue;
            return LoongValue::string(std::string(1, static_cast<char>(value & 255)));
        }
    ));
    
    // bytes_from_int16(value) - 将整数转换为小端2字节字符串
    globals_->define("bytes_from_int16", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("bytes_from_int16() 需要一个整数参数");
            }
            int value = (int)args[0].numberValue;
            std::string result;
            result += static_cast<char>(value & 255);
            result += static_cast<char>((value >> 8) & 255);
            return LoongValue::string(result);
        }
    ));
    
    // bytes_from_int24(value) - 将整数转换为小端3字节字符串
    globals_->define("bytes_from_int24", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("bytes_from_int24() 需要一个整数参数");
            }
            int value = (int)args[0].numberValue;
            std::string result;
            result += static_cast<char>(value & 255);
            result += static_cast<char>((value >> 8) & 255);
            result += static_cast<char>((value >> 16) & 255);
            return LoongValue::string(result);
        }
    ));
    
    // bytes_from_int32(value) - 将整数转换为小端4字节字符串
    globals_->define("bytes_from_int32", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isNumber()) {
                throw std::runtime_error("bytes_from_int32() 需要一个整数参数");
            }
            long long value = (long long)args[0].numberValue;
            std::string result;
            result += static_cast<char>(value & 255);
            result += static_cast<char>((value >> 8) & 255);
            result += static_cast<char>((value >> 16) & 255);
            result += static_cast<char>((value >> 24) & 255);
            return LoongValue::string(result);
        }
    ));
    
    // bytes_to_hex(data) - 将二进制数据转换为十六进制字符串
    globals_->define("bytes_to_hex", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("bytes_to_hex() 需要一个字符串参数");
            }
            const std::string& data = args[0].stringValue;
            std::string result;
            const char hex[] = "0123456789abcdef";
            for (size_t i = 0; i < data.size(); i++) {
                unsigned char c = static_cast<unsigned char>(data[i]);
                result += hex[(c >> 4) & 15];
                result += hex[c & 15];
            }
            return LoongValue::string(result);
        }
    ));
    
    // bytes_from_hex(hex) - 将十六进制字符串转换为二进制数据
    globals_->define("bytes_from_hex", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("bytes_from_hex() 需要一个字符串参数");
            }
            const std::string& hex = args[0].stringValue;
            if (hex.size() % 2 != 0) {
                throw std::runtime_error("bytes_from_hex() 十六进制字符串长度必须为偶数");
            }
            std::string result;
            for (size_t i = 0; i < hex.size(); i += 2) {
                int hi = 0, lo = 0;
                char ch = hex[i];
                if (ch >= '0' && ch <= '9') hi = ch - '0';
                else if (ch >= 'a' && ch <= 'f') hi = ch - 'a' + 10;
                else if (ch >= 'A' && ch <= 'F') hi = ch - 'A' + 10;
                ch = hex[i + 1];
                if (ch >= '0' && ch <= '9') lo = ch - '0';
                else if (ch >= 'a' && ch <= 'f') lo = ch - 'a' + 10;
                else if (ch >= 'A' && ch <= 'F') lo = ch - 'A' + 10;
                result += static_cast<char>((hi << 4) | lo);
            }
            return LoongValue::string(result);
        }
    ));
    
    // bytes_concat(data1, data2) - 连接两个二进制数据
    globals_->define("bytes_concat", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("bytes_concat() 需要两个字符串参数");
            }
            return LoongValue::string(args[0].stringValue + args[1].stringValue);
        }
    ));
    
    // bytes_substr(data, start, len) - 截取二进制数据的子串
    globals_->define("bytes_substr", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isNumber()) {
                throw std::runtime_error("bytes_substr() 需要 data(string), start(number), [len(number)] 参数");
            }
            const std::string& data = args[0].stringValue;
            int start = (int)args[1].numberValue;
            int len = (int)data.size() - start;
            if (args.size() > 2 && args[2].isNumber()) {
                len = (int)args[2].numberValue;
            }
            if (start < 0 || start >= (int)data.size()) {
                return LoongValue::string("");
            }
            if (start + len > (int)data.size()) {
                len = (int)data.size() - start;
            }
            return LoongValue::string(data.substr(start, len));
        }
    ));
    
    // bytes_null() - 返回一个null字节
    globals_->define("bytes_null", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            return LoongValue::string(std::string(1, '\0'));
        }
    ));
    
    // ==================== SHA1 哈希函数 ====================
    // SHA1实现（纯C++，用于MySQL mysql_native_password认证等场景）
    // SHA1算法: https://tools.ietf.org/html/rfc3174
    
    // sha1(data) - 计算SHA1哈希，返回20字节二进制数据
    globals_->define("sha1", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.empty() || !args[0].isString()) {
                throw std::runtime_error("sha1() 需要一个字符串参数");
            }
            const std::string& data = args[0].stringValue;
            
            // SHA1常量
            uint32_t h0 = 0x67452301;
            uint32_t h1 = 0xEFCDAB89;
            uint32_t h2 = 0x98BADCFE;
            uint32_t h3 = 0x10325476;
            uint32_t h4 = 0xC3D2E1F0;
            
            uint64_t bitLen = data.size() * 8;
            
            // 填充消息
            std::string msg = data;
            msg += static_cast<char>(0x80);
            while ((msg.size() % 64) != 56) {
                msg += static_cast<char>(0x00);
            }
            // 追加原始长度（大端64位）
            for (int i = 7; i >= 0; --i) {
                msg += static_cast<char>((bitLen >> (i * 8)) & 0xFF);
            }
            
            // 处理每个512位块
            for (size_t offset = 0; offset < msg.size(); offset += 64) {
                uint32_t w[80];
                for (int i = 0; i < 16; ++i) {
                    w[i] = (static_cast<uint32_t>(static_cast<unsigned char>(msg[offset + i*4])) << 24) |
                            (static_cast<uint32_t>(static_cast<unsigned char>(msg[offset + i*4+1])) << 16) |
                            (static_cast<uint32_t>(static_cast<unsigned char>(msg[offset + i*4+2])) << 8) |
                            (static_cast<uint32_t>(static_cast<unsigned char>(msg[offset + i*4+3])));
                }
                for (int i = 16; i < 80; ++i) {
                    uint32_t tmp = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
                    w[i] = (tmp << 1) | (tmp >> 31);
                }
                
                uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
                
                for (int i = 0; i < 80; ++i) {
                    uint32_t f, k;
                    if (i < 20) {
                        f = (b & c) | ((~b) & d);
                        k = 0x5A827999;
                    } else if (i < 40) {
                        f = b ^ c ^ d;
                        k = 0x6ED9EBA1;
                    } else if (i < 60) {
                        f = (b & c) | (b & d) | (c & d);
                        k = 0x8F1BBCDC;
                    } else {
                        f = b ^ c ^ d;
                        k = 0xCA62C1D6;
                    }
                    uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
                    e = d;
                    d = c;
                    c = (b << 30) | (b >> 2);
                    b = a;
                    a = temp;
                }
                
                h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
            }
            
            // 输出20字节哈希（大端）
            std::string result;
            uint32_t hashes[5] = {h0, h1, h2, h3, h4};
            for (int i = 0; i < 5; ++i) {
                result += static_cast<char>((hashes[i] >> 24) & 0xFF);
                result += static_cast<char>((hashes[i] >> 16) & 0xFF);
                result += static_cast<char>((hashes[i] >> 8) & 0xFF);
                result += static_cast<char>(hashes[i] & 0xFF);
            }
            return LoongValue::string(result);
        }
    ));
    
    // bytes_xor(data1, data2) - 对两个字节数组进行XOR运算（用于MySQL认证等）
    globals_->define("bytes_xor", LoongValue::builtinFunction(
        [](const std::vector<LoongValue>& args) -> LoongValue {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                throw std::runtime_error("bytes_xor() 需要两个等长字符串参数");
            }
            const std::string& a = args[0].stringValue;
            const std::string& b = args[1].stringValue;
            if (a.size() != b.size()) {
                throw std::runtime_error("bytes_xor() 两个参数长度必须相同");
            }
            std::string result;
            result.resize(a.size());
            for (size_t i = 0; i < a.size(); ++i) {
                result[i] = a[i] ^ b[i];
            }
            return LoongValue::string(result);
        }
    ));
    
    // channel内置函数 - 创建通道
    globals_->define("channel", LoongValue::builtinFunction([](const std::vector<LoongValue>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("channel函数需要一个参数: channel(capacity)");
        }
        if (!args[0].isNumber()) {
            throw std::runtime_error("channel容量必须是数字");
        }
        
        int capacity = static_cast<int>(args[0].numberValue);
        auto channel = std::make_shared<ChannelValue>(capacity);
        return LoongValue::channel(channel);
    }));
    
    // mutex内置函数 - 创建互斥锁
    globals_->define("mutex", LoongValue::builtinFunction([](const std::vector<LoongValue>& args) {
        if (!args.empty()) {
            throw std::runtime_error("mutex函数不需要参数");
        }
        
        auto mutex = std::make_shared<MutexValue>();
        return LoongValue::mutex(mutex);
    }));
}

} // namespace loong
