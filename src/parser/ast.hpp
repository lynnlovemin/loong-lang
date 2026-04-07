#ifndef LOONG_AST_HPP
#define LOONG_AST_HPP

#include "lexer/token.hpp"
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <map>

namespace loong {

// 前向声明
class Expr;
class Stmt;

// 使用智能指针
using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

// 表达式基类
class Expr {
public:
    virtual ~Expr() = default;
    int line = 1;
    int column = 1;
};

// 语句基类
class Stmt {
public:
    virtual ~Stmt() = default;
    int line = 1;
    int column = 1;
};

// ==================== 表达式节点 ====================

// 字面量表达式
class LiteralExpr : public Expr {
public:
    enum class LiteralType { NIL, BOOL, NUMBER, BIGINT, STRING };
    LiteralType literalType;
    double numberValue;
    std::string stringValue;
    std::string bigintString;  // 存储大整数的字符串形式
    bool boolValue;
    
    static ExprPtr createNil(int line = 1, int column = 1) {
        auto expr = std::make_shared<LiteralExpr>();
        expr->literalType = LiteralType::NIL;
        expr->line = line;
        expr->column = column;
        return expr;
    }
    
    static ExprPtr createBool(bool value, int line = 1, int column = 1) {
        auto expr = std::make_shared<LiteralExpr>();
        expr->literalType = LiteralType::BOOL;
        expr->boolValue = value;
        expr->line = line;
        expr->column = column;
        return expr;
    }
    
    static ExprPtr createNumber(double value, int line = 1, int column = 1) {
        auto expr = std::make_shared<LiteralExpr>();
        expr->literalType = LiteralType::NUMBER;
        expr->numberValue = value;
        expr->line = line;
        expr->column = column;
        return expr;
    }
    
    static ExprPtr createBigint(const std::string& value, int line = 1, int column = 1) {
        auto expr = std::make_shared<LiteralExpr>();
        expr->literalType = LiteralType::BIGINT;
        expr->bigintString = value;
        expr->line = line;
        expr->column = column;
        return expr;
    }
    
    static ExprPtr createString(const std::string& value, int line = 1, int column = 1) {
        auto expr = std::make_shared<LiteralExpr>();
        expr->literalType = LiteralType::STRING;
        expr->stringValue = value;
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// 标识符表达式
class IdentifierExpr : public Expr {
public:
    std::string name;
    
    static ExprPtr create(const std::string& name, int line = 1, int column = 1) {
        auto expr = std::make_shared<IdentifierExpr>();
        expr->name = name;
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// 二元表达式
class BinaryExpr : public Expr {
public:
    ExprPtr left;
    Token op;
    ExprPtr right;
    
    static ExprPtr create(ExprPtr left, Token op, ExprPtr right) {
        auto expr = std::make_shared<BinaryExpr>();
        expr->left = left;
        expr->op = op;
        expr->right = right;
        expr->line = op.line;
        expr->column = op.column;
        return expr;
    }
};

// 一元表达式
class UnaryExpr : public Expr {
public:
    Token op;
    ExprPtr operand;
    
    static ExprPtr create(Token op, ExprPtr operand) {
        auto expr = std::make_shared<UnaryExpr>();
        expr->op = op;
        expr->operand = operand;
        expr->line = op.line;
        expr->column = op.column;
        return expr;
    }
};

// 分组表达式
class GroupedExpr : public Expr {
public:
    ExprPtr expression;
    
    static ExprPtr create(ExprPtr expression, int line = 1, int column = 1) {
        auto expr = std::make_shared<GroupedExpr>();
        expr->expression = expression;
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// 列表表达式
class ListExpr : public Expr {
public:
    std::vector<ExprPtr> elements;
    
    static ExprPtr create(const std::vector<ExprPtr>& elements, int line = 1, int column = 1) {
        auto expr = std::make_shared<ListExpr>();
        expr->elements = elements;
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// 字典表达式
class DictExpr : public Expr {
public:
    std::vector<std::pair<ExprPtr, ExprPtr>> pairs;
    
    static ExprPtr create(const std::vector<std::pair<ExprPtr, ExprPtr>>& pairs, int line = 1, int column = 1) {
        auto expr = std::make_shared<DictExpr>();
        expr->pairs = pairs;
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// 索引访问表达式
class IndexExpr : public Expr {
public:
    ExprPtr object;
    ExprPtr index;
    
    static ExprPtr create(ExprPtr object, ExprPtr index, int line = 1, int column = 1) {
        auto expr = std::make_shared<IndexExpr>();
        expr->object = object;
        expr->index = index;
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// 成员访问表达式
class MemberExpr : public Expr {
public:
    ExprPtr object;
    std::string member;
    
    static ExprPtr create(ExprPtr object, const std::string& member, int line = 1, int column = 1) {
        auto expr = std::make_shared<MemberExpr>();
        expr->object = object;
        expr->member = member;
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// 函数调用表达式
class CallExpr : public Expr {
public:
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
    
    static ExprPtr create(ExprPtr callee, const std::vector<ExprPtr>& args, int line = 1, int column = 1) {
        auto expr = std::make_shared<CallExpr>();
        expr->callee = callee;
        expr->arguments = args;
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// 赋值表达式
class AssignExpr : public Expr {
public:
    ExprPtr target;  // 可以是标识符、索引访问或成员访问
    ExprPtr value;
    
    static ExprPtr create(ExprPtr target, ExprPtr value, int line = 1, int column = 1) {
        auto expr = std::make_shared<AssignExpr>();
        expr->target = target;
        expr->value = value;
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// ==================== 语句节点 ====================

// 表达式语句
class ExprStmt : public Stmt {
public:
    ExprPtr expression;
    
    static StmtPtr create(ExprPtr expr, int line = 1, int column = 1) {
        auto stmt = std::make_shared<ExprStmt>();
        stmt->expression = expr;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// 变量声明语句
class ValStmt : public Stmt {
public:
    std::string name;
    ExprPtr initializer;
    
    static StmtPtr create(const std::string& name, ExprPtr init, int line = 1, int column = 1) {
        auto stmt = std::make_shared<ValStmt>();
        stmt->name = name;
        stmt->initializer = init;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// 函数声明语句
class FnStmt : public Stmt {
public:
    std::string name;
    std::vector<std::string> params;
    std::vector<std::pair<std::string, ExprPtr>> defaultParams;  // 默认参数
    std::vector<StmtPtr> body;
    
    static StmtPtr create(const std::string& name, 
                          const std::vector<std::string>& params,
                          const std::vector<std::pair<std::string, ExprPtr>>& defaultParams,
                          const std::vector<StmtPtr>& body,
                          int line = 1, int column = 1) {
        auto stmt = std::make_shared<FnStmt>();
        stmt->name = name;
        stmt->params = params;
        stmt->defaultParams = defaultParams;
        stmt->body = body;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// Return语句
class ReturnStmt : public Stmt {
public:
    ExprPtr value;
    
    static StmtPtr create(ExprPtr value, int line = 1, int column = 1) {
        auto stmt = std::make_shared<ReturnStmt>();
        stmt->value = value;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// If语句
class IfStmt : public Stmt {
public:
    ExprPtr condition;
    std::vector<StmtPtr> thenBranch;
    std::vector<StmtPtr> elseBranch;  // 可能为空
    
    static StmtPtr create(ExprPtr condition,
                          const std::vector<StmtPtr>& thenBranch,
                          const std::vector<StmtPtr>& elseBranch,
                          int line = 1, int column = 1) {
        auto stmt = std::make_shared<IfStmt>();
        stmt->condition = condition;
        stmt->thenBranch = thenBranch;
        stmt->elseBranch = elseBranch;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// While语句
class WhileStmt : public Stmt {
public:
    ExprPtr condition;
    std::vector<StmtPtr> body;
    
    static StmtPtr create(ExprPtr condition, const std::vector<StmtPtr>& body, int line = 1, int column = 1) {
        auto stmt = std::make_shared<WhileStmt>();
        stmt->condition = condition;
        stmt->body = body;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// For语句（传统for循环）
class ForStmt : public Stmt {
public:
    StmtPtr initializer;    // 可能为空
    ExprPtr condition;      // 可能为空
    ExprPtr increment;      // 可能为空
    std::vector<StmtPtr> body;
    
    static StmtPtr create(StmtPtr init, ExprPtr cond, ExprPtr incr,
                          const std::vector<StmtPtr>& body,
                          int line = 1, int column = 1) {
        auto stmt = std::make_shared<ForStmt>();
        stmt->initializer = init;
        stmt->condition = cond;
        stmt->increment = incr;
        stmt->body = body;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// For-in语句（遍历循环）
class ForInStmt : public Stmt {
public:
    std::string varName;
    ExprPtr iterable;
    std::vector<StmtPtr> body;
    
    static StmtPtr create(const std::string& varName, ExprPtr iterable,
                          const std::vector<StmtPtr>& body,
                          int line = 1, int column = 1) {
        auto stmt = std::make_shared<ForInStmt>();
        stmt->varName = varName;
        stmt->iterable = iterable;
        stmt->body = body;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// Break语句
class BreakStmt : public Stmt {
public:
    static StmtPtr create(int line = 1, int column = 1) {
        auto stmt = std::make_shared<BreakStmt>();
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// Continue语句
class ContinueStmt : public Stmt {
public:
    static StmtPtr create(int line = 1, int column = 1) {
        auto stmt = std::make_shared<ContinueStmt>();
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// Import语句
class ImportStmt : public Stmt {
public:
    std::string moduleName;
    std::string filePath;  // 如果指定了from，则为文件路径
    
    static StmtPtr create(const std::string& moduleName, const std::string& filePath = "", int line = 1, int column = 1) {
        auto stmt = std::make_shared<ImportStmt>();
        stmt->moduleName = moduleName;
        stmt->filePath = filePath;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// Try语句
class TryStmt : public Stmt {
public:
    std::vector<StmtPtr> tryBlock;
    std::string catchVar;           // catch变量名
    std::vector<StmtPtr> catchBlock;
    std::vector<StmtPtr> finallyBlock;  // 可能为空
    
    static StmtPtr create(const std::vector<StmtPtr>& tryBlock,
                          const std::string& catchVar,
                          const std::vector<StmtPtr>& catchBlock,
                          const std::vector<StmtPtr>& finallyBlock,
                          int line = 1, int column = 1) {
        auto stmt = std::make_shared<TryStmt>();
        stmt->tryBlock = tryBlock;
        stmt->catchVar = catchVar;
        stmt->catchBlock = catchBlock;
        stmt->finallyBlock = finallyBlock;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// Throw语句
class ThrowStmt : public Stmt {
public:
    ExprPtr value;  // 抛出的异常值
    
    static StmtPtr create(ExprPtr value, int line = 1, int column = 1) {
        auto stmt = std::make_shared<ThrowStmt>();
        stmt->value = value;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// Class语句
class ClassStmt : public Stmt {
public:
    std::string name;
    std::string superClass;     // 父类名（可为空）
    std::vector<StmtPtr> body;  // 类体（方法定义）
    
    static StmtPtr create(const std::string& name,
                          const std::string& superClass,
                          const std::vector<StmtPtr>& body,
                          int line = 1, int column = 1) {
        auto stmt = std::make_shared<ClassStmt>();
        stmt->name = name;
        stmt->superClass = superClass;
        stmt->body = body;
        stmt->line = line;
        stmt->column = column;
        return stmt;
    }
};

// This表达式
class ThisExpr : public Expr {
public:
    static ExprPtr create(int line = 1, int column = 1) {
        auto expr = std::make_shared<ThisExpr>();
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// Super表达式
class SuperExpr : public Expr {
public:
    std::string method;  // 调用的父类方法
    
    static ExprPtr create(const std::string& method, int line = 1, int column = 1) {
        auto expr = std::make_shared<SuperExpr>();
        expr->method = method;
        expr->line = line;
        expr->column = column;
        return expr;
    }
};

// 程序根节点
class Program {
public:
    std::vector<StmtPtr> statements;
    
    void addStatement(StmtPtr stmt) {
        statements.push_back(stmt);
    }
};

} // namespace loong

#endif // LOONG_AST_HPP
